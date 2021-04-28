#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include <twz/gate.h>
#include <twz/obj.h>
#include <twz/queue.h>
#include <twz/sec/security.h>
#include <twz/sys/obj.h>
#include <twz/sys/thread.h>

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include <twix/twix.h>

#include "state.h"

#define HANDLER_QUEUE_ADD_CLIENT 0
#define HANDLER_QUEUE_DEL_CLIENT 1

struct handler_queue_entry {
	struct queue_entry qe;
	int cmd;
	size_t client_idx;
};

class handler
{
  public:
	handler()
	{
		int r = twz_object_new(
		  &client_add_queue, NULL, NULL, OBJ_VOLATILE, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE);
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

	void add_client(std::shared_ptr<queue_client> client)
	{
		struct handler_queue_entry subhqe;
		subhqe.cmd = HANDLER_QUEUE_ADD_CLIENT;
		std::lock_guard<std::mutex> _lg(lock);
		add_clients.push_back(client);
		queue_submit(&client_add_queue, (struct queue_entry *)&subhqe, 0);
		waker = 1;
		twz_thread_sync(THREAD_SYNC_WAKE, &waker, 1, NULL);
	}

  private:
	twzobj client_add_queue;
	std::vector<std::shared_ptr<queue_client>> clients, add_clients;
	std::thread thread, cleanup_thread;
	std::mutex lock;
	std::condition_variable cv;
	struct queue_dequeue_multiple_spec *qspec = nullptr;
	size_t qspec_len = 0;
	struct handler_queue_entry hqe;
	struct twix_queue_entry *tqe = nullptr;
	std::atomic<unsigned long> waker = 0;

	void handler_cleanup_thread()
	{
		struct sys_thread_sync_args *tsa = nullptr;
		size_t tsa_len = 0;
		for(;;) {
			size_t len, sleep_count = 0;
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
					tsa = new sys_thread_sync_args[len + 1];
					tsa_len = len;
				}
				for(size_t i = 0; i < len; i++) {
					struct twzthread_repr *repr =
					  (struct twzthread_repr *)twz_object_base(&clients[i]->thrdobj);
					if(repr->syncs[THRD_SYNC_EXIT] == 0) {
						tsa[i].res = 0;
						sleep_count++;
					}
					twz_thread_sync_init(
					  &tsa[i], THREAD_SYNC_SLEEP, &repr->syncs[THRD_SYNC_EXIT], 0);
				}
			}
			twz_thread_sync_init(&tsa[len], THREAD_SYNC_SLEEP, &waker, 0);
			tsa[len].res = 0;
			if(sleep_count == len) {
				int r = twz_thread_sync_multiple(len + 1, tsa, NULL);
				(void)r;
				/* TODO err */
			}
			tsa[len].res = 0;

			{
				std::lock_guard<std::mutex> _lg(lock);
				waker = 0;
				if(len > clients.size()) {
					len = clients.size();
				}
				for(size_t i = 0; i < len; i++) {
					struct twzthread_repr *repr =
					  (struct twzthread_repr *)twz_object_base(&clients[i]->thrdobj);
					if(repr->syncs[THRD_SYNC_EXIT]) {
						tsa[i].res = 1;
					} else {
						tsa[i].res = 0;
					}
				}
			}
			for(size_t i = 0; i < len; i++) {
				if(tsa[i].res) {
					/* that thread exited, inform the main thread */
					struct handler_queue_entry subhqe;
					subhqe.cmd = HANDLER_QUEUE_DEL_CLIENT;
					subhqe.client_idx = i;
					queue_submit(&client_add_queue, (struct queue_entry *)&subhqe, 0);
					queue_get_finished(&client_add_queue, (struct queue_entry *)&subhqe, 0);
					break;
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

	void handle_client(std::shared_ptr<queue_client> client,
	  struct twix_queue_entry *tqe,
	  bool drain)
	{
		/*
		fprintf(stderr,
		  "handle client: %p: %d %ld %ld %ld %ld %ld %ld %ld %d\n",
		  client,
		  tqe->cmd,
		  tqe->arg0,
		  tqe->arg1,
		  tqe->arg2,
		  tqe->arg3,
		  tqe->arg4,
		  tqe->arg5,
		  tqe->buflen,
		  tqe->flags);
		  */
		auto [ret, respond] = handle_command(client, tqe);
		if(!drain && respond) {
			tqe->ret = ret;
			//		fprintf(stderr, "respond COMPLETING %d\n", tqe->qe.info);
			/* TODO: make this non-blocking, and handle the case where it wants to block */
			queue_complete(&client->queue, (struct queue_entry *)tqe, 0);
		}
	}

	void handle_handler_queue(struct handler_queue_entry *hqe)
	{
		switch(hqe->cmd) {
			case HANDLER_QUEUE_ADD_CLIENT: {
				std::lock_guard<std::mutex> _lg(lock);
				for(auto client : add_clients) {
					clients.push_back(client);
					//		fprintf(stderr, "added client: %d\n", client->proc->pid);
				}
				add_clients.clear();
				cv.notify_all();
				//		fprintf(stderr, "added client %p -> %ld\n", hqe->client, clients.size() -
				// 1);
			} break;
			case HANDLER_QUEUE_DEL_CLIENT: {
				std::shared_ptr<queue_client> client;
				{
					std::lock_guard<std::mutex> _lg(lock);
					client = clients[hqe->client_idx];
					//	fprintf(stderr, "erase %ld / %ld\n", hqe->client_idx, clients.size());
					clients.erase(clients.begin() + hqe->client_idx);
					queue_complete(&client_add_queue, (struct queue_entry *)hqe, 0);
				}
				struct twix_queue_entry tqe;
				while(
				  queue_receive(&client->queue, (struct queue_entry *)&tqe, QUEUE_NONBLOCK) == 0) {
					handle_client(client, &tqe, true);
				}
				// fprintf(stderr, "removed client: %d : %p\n", client->proc->pid, client.get());
				client->exit();
			} break;
		}
		qspec_build();
	}

	void handler_thread()
	{
		for(;;) {
			// debug_printf("WAITING\n");
			ssize_t ret = queue_sub_dequeue_multiple(qspec_len, qspec);
			// debug_printf("HANDLING\n");
			/* TODO: check ret */
			(void)ret;
			for(size_t i = 1; i < qspec_len; i++) {
				if(qspec[i].ret != 0) {
					/* got a message */
					std::shared_ptr<queue_client> client;
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
DECLARE_SAPI_ENTRY(open_queue, TWIX_GATE_OPEN_QUEUE, int, int flags, objid_t *qid, objid_t *bid)
{
	(void)flags;
	std::shared_ptr<queue_client> client = std::make_shared<queue_client>();
	twz_object_init_guid(&client->thrdobj, twz_thread_repr_base()->reprid, FE_READ);
	int r = twz_object_wire(NULL, &client->thrdobj);
	(void)r;
	r = client_init(client);
	if(r) {
		return r;
	}

	{
		std::lock_guard<std::mutex> _lg(handlers_lock);
		handler *handler = handlers[0]; // TODO
		handler->add_client(client);
	}
	*qid = twz_object_guid(&client->queue);
	*bid = twz_object_guid(&client->buffer);
	return 0;
}
}

#include "async.h"
#include <sys/stat.h>
#include <twz/name.h>
int main()
{
	twzobj api_obj;
	int r = twz_object_new(&api_obj, NULL, NULL, OBJ_VOLATILE, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE);
	if(r) {
		abort();
	}
	twz_secure_api_create(&api_obj, "twix-unix");
	// struct secure_api_header *sah = (struct secure_api_header *)twz_object_base(&api_obj);

	auto h = new handler();
	handlers.push_back(h);

	async_init();
	mkdir("/dev", 0644);
	twz_name_assign(twz_object_guid(&api_obj), "/dev/unix");

	for(;;) {
		sleep(100);
	}
}
