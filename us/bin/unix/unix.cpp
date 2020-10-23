#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include <twz/gate.h>
#include <twz/obj.h>
#include <twz/queue.h>
#include <twz/security.h>

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include <twix/twix.h>

class queue_client
{
  public:
	twzobj queue, thrdobj;
	queue_client()
	{
	}

	int init()
	{
		int r =
		  twz_object_new(&queue, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE);
		if(r)
			return r;
		r = queue_init_hdr(
		  &queue, 12, sizeof(struct twix_queue_entry), 12, sizeof(struct twix_queue_entry));
		return r;
	}
};

#define HANDLER_QUEUE_ADD_CLIENT 0
#define HANDLER_QUEUE_DEL_CLIENT 1

struct handler_queue_entry {
	struct queue_entry qe;
	int cmd;
	union {
		queue_client *client;
		size_t client_idx;
	};
};

class handler
{
  public:
	handler()
	{
		int r = twz_object_new(&client_add_queue, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE);
		if(r)
			throw(r);
		r = queue_init_hdr(&client_add_queue,
		  8,
		  sizeof(struct handler_queue_entry),
		  8,
		  sizeof(struct handler_queue_entry));
		if(r)
			throw(r);

		qspec_build();
		thread = std::thread(&handler::handler_thread, this);
		cleanup_thread = std::thread(&handler::handler_cleanup_thread, this);
	}

	void add_client(queue_client *client)
	{
		struct handler_queue_entry subhqe;
		subhqe.client = client;
		subhqe.cmd = HANDLER_QUEUE_ADD_CLIENT;
		queue_submit(&client_add_queue, (struct queue_entry *)&subhqe, 0);
	}

  private:
	twzobj client_add_queue;
	std::vector<queue_client *> clients;
	std::thread thread, cleanup_thread;
	std::mutex lock;
	std::condition_variable cv;
	struct queue_dequeue_multiple_spec *qspec = nullptr;
	size_t qspec_len = 0;
	struct handler_queue_entry hqe;
	struct twix_queue_entry *tqe = nullptr;

	void handler_cleanup_thread()
	{
		struct sys_thread_sync_args *tsa;
		size_t tsa_len = 0;
		for(;;) {
			size_t len;
			{
				std::unique_lock<std::mutex> _lg(lock);
				if(clients.size() == 0) {
					cv.wait(_lg);
				}
				len = clients.size();
				if(len > tsa_len) {
					if(tsa) {
						delete[] tsa;
					}
					tsa = new sys_thread_sync_args[len];
					tsa_len = len;
				}
				for(size_t i = 0; i < len; i++) {
					struct twzthread_repr *repr =
					  (struct twzthread_repr *)twz_object_base(&clients[i]->thrdobj);
					twz_thread_sync_init(
					  &tsa[i], THREAD_SYNC_SLEEP, &repr->syncs[THRD_SYNC_EXIT], 0);
				}
			}
			int r = twz_thread_sync_multiple(len, tsa, NULL);
			/* TODO err */

			{
				std::lock_guard<std::mutex> _lg(lock);
				if(len > clients.size()) {
					len = clients.size();
				}
			}
			for(size_t i = 0; i < len; i++) {
				struct twzthread_repr *repr =
				  (struct twzthread_repr *)twz_object_base(&clients[i]->thrdobj);
				if(repr->syncs[THRD_SYNC_EXIT]) {
					/* that thread exited, inform the main thread */
					struct handler_queue_entry subhqe;
					subhqe.cmd = HANDLER_QUEUE_DEL_CLIENT;
					subhqe.client_idx = i;
					queue_submit(&client_add_queue, (struct queue_entry *)&subhqe, 0);
					queue_get_finished(&client_add_queue, (struct queue_entry *)&subhqe, 0);
				}
			}
		}
	}

	void qspec_build()
	{
		std::lock_guard<std::mutex> _lg(lock);
		size_t needed = clients.size() + 1;
		if(needed > qspec_len || !qspec || !tqe) {
			if(qspec) {
				delete[] qspec;
			}
			qspec = new queue_dequeue_multiple_spec[needed];
			if(tqe) {
				delete[] tqe;
			}
			tqe = new twix_queue_entry[needed - 1];
		}
		qspec[0].obj = &client_add_queue;
		qspec[0].result = (struct queue_entry *)&hqe;
		qspec[0].sq = SUBQUEUE_SUBM;

		size_t i = 1;
		for(auto client : clients) {
			qspec[i].obj = &client->queue;
			qspec[i].result = (struct queue_entry *)&tqe[i - 1];
			qspec[i].sq = SUBQUEUE_SUBM;
			i++;
		}
		qspec_len = needed;
	}

	void handle_client(queue_client *client, struct twix_queue_entry *tqe, bool drain)
	{
		fprintf(stderr, "handle client: %d\n", tqe->x);
		queue_complete(&client->queue, (struct queue_entry *)tqe, 0);
	}

	void handle_handler_queue(struct handler_queue_entry *hqe)
	{
		switch(hqe->cmd) {
			case HANDLER_QUEUE_ADD_CLIENT: {
				std::lock_guard<std::mutex> _lg(lock);
				clients.push_back(hqe->client);
				cv.notify_all();
				//		fprintf(stderr, "added client %p -> %ld\n", hqe->client, clients.size() -
				// 1);
			} break;
			case HANDLER_QUEUE_DEL_CLIENT: {
				queue_client *client;
				{
					std::lock_guard<std::mutex> _lg(lock);
					client = clients[hqe->client_idx];
					clients.erase(clients.begin() + hqe->client_idx);
					queue_complete(&client_add_queue, (struct queue_entry *)hqe, 0);
				}
				struct twix_queue_entry tqe;
				while(
				  queue_receive(&client->queue, (struct queue_entry *)&tqe, QUEUE_NONBLOCK) == 0) {
					handle_client(client, &tqe, true);
				}
				delete client;
				//		fprintf(stderr, "deleted client %ld\n", hqe->client_idx);
			} break;
		}
		qspec_build();
	}

	void handler_thread()
	{
		for(;;) {
			ssize_t ret = queue_sub_dequeue_multiple(qspec_len, qspec);
			for(size_t i = 1; i < qspec_len; i++) {
				// fprintf(stderr, "   -> %ld: %d\n", i, qspec[i].ret);
				if(qspec[i].ret != 0) {
					/* got a message */
					queue_client *client;
					{
						std::lock_guard<std::mutex> _lg(lock);
						client = clients[i - 1];
					}
					handle_client(client, (struct twix_queue_entry *)qspec[i].result, false);
				}
			}
			if(qspec[0].ret != 0) {
				handle_handler_queue((struct handler_queue_entry *)qspec[0].result);
			}
		}
	}
};

std::vector<handler *> handlers;
std::mutex handlers_lock;

extern "C" {
DECLARE_SAPI_ENTRY(open_queue, TWIX_GATE_OPEN_QUEUE, int, int flags, objid_t *arg)
{
	fprintf(stderr, "hello from queue open\n");

	queue_client *client = new queue_client();
	int r = client->init();
	if(r) {
		delete client;
		return r;
	}

	twz_object_init_guid(&client->thrdobj, twz_thread_repr_base()->reprid, FE_READ);

	{
		std::lock_guard<std::mutex> _lg(handlers_lock);
		handler *handler = handlers[0]; // TODO
		handler->add_client(client);
	}
	*arg = twz_object_guid(&client->queue);
	return 0;
}
}

#include <twz/name.h>
int main()
{
	twzobj api_obj;
	twz_object_new(&api_obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE);
	twz_secure_api_create(&api_obj, "twix-unix");
	struct secure_api_header *sah = (struct secure_api_header *)twz_object_base(&api_obj);

	auto h = new handler();
	handlers.push_back(h);

	twz_name_assign(twz_object_guid(&api_obj), "/dev/unix");

	for(;;) {
		sleep(100);
	}
}
