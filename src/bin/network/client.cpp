#include <stdlib.h>
#include <twz/gate.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/queue.h>
#include <twz/security.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <nstack/nstack.h>

#include "client.h"

/* Don't worry about this complexity, it's actually something I'm planning on making a
 * system-provided API for, and this will be cleaned up */
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
		int r = twz_object_new(&client_add_queue,
		  NULL,
		  NULL,
		  TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE | TWZ_OC_TIED_NONE);
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

	void add_client(std::shared_ptr<net_client> client)
	{
		fprintf(stderr, "adding client : %p\n", client.get());
		struct handler_queue_entry subhqe;
		subhqe.cmd = HANDLER_QUEUE_ADD_CLIENT;
		std::lock_guard<std::mutex> _lg(lock);
		adding_clients.push_back(client);
		queue_submit(&client_add_queue, (struct queue_entry *)&subhqe, 0);
	}

  private:
	twzobj client_add_queue;
	std::vector<std::shared_ptr<net_client>> clients;
	std::vector<std::shared_ptr<net_client>> adding_clients;
	std::thread thread, cleanup_thread;
	std::mutex lock;
	std::condition_variable cv;
	struct queue_dequeue_multiple_spec *qspec = nullptr;
	size_t qspec_len = 0;
	struct handler_queue_entry hqe;
	struct nstack_queue_entry *nqe = nullptr;

	void handler_cleanup_thread()
	{
		kso_set_name(NULL, "net_client_handler::cleanup");
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
					tsa = new sys_thread_sync_args[len];
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
			if(sleep_count == len) {
				int r = twz_thread_sync_multiple(len, tsa, NULL);
				(void)r;
				/* TODO err */
			}

			{
				std::lock_guard<std::mutex> _lg(lock);
				if(len > clients.size()) {
					len = clients.size();
				}
				for(size_t i = 0; i < len; i++) {
					struct twzthread_repr *repr =
					  (struct twzthread_repr *)twz_object_base(&clients[i]->thrdobj);
					if(repr->syncs[THRD_SYNC_EXIT]) {
						tsa[i].res = 1;
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
		size_t needed = clients.size() * 2 + 1;
		if(needed > qspec_len || !qspec || !nqe) {
			if(qspec) {
				delete[] qspec;
			}
			qspec = new queue_dequeue_multiple_spec[needed];
			if(nqe) {
				delete[] nqe;
			}
			nqe = new nstack_queue_entry[needed - 1];
		}
		qspec[0].obj = &client_add_queue;
		qspec[0].result = (struct queue_entry *)&hqe;
		qspec[0].sq = SUBQUEUE_SUBM;

		size_t i = 1;
		for(auto client : clients) {
			/* odd indicies are tx, even are rx */
			qspec[i].obj = &client->txq_obj;
			qspec[i].result = (struct queue_entry *)&nqe[i - 1];
			qspec[i].sq = SUBQUEUE_SUBM;
			i++;

			qspec[i].obj = &client->rxq_obj;
			qspec[i].result = (struct queue_entry *)&nqe[i - 1];
			qspec[i].sq = SUBQUEUE_CMPL;
			i++;
		}
		qspec_len = needed;
	}

	void handle_client(std::shared_ptr<net_client> client,
	  struct nstack_queue_entry *nqe,
	  bool drain,
	  bool is_incoming)
	{
		if(is_incoming) {
			uint32_t info = nqe->qe.info;
			if(handle_command(client, nqe)) {
				/* TODO: make this non-blocking, and handle the case where it wants to block */
				if(nqe->qe.info != info) {
					fprintf(stderr, "WARNING - incorrect info\n");
				}
				if(drain)
					return;
				// fprintf(stderr, ":: %d %d %x\n", nqe->qe.info, info, nqe->qe.cmd_id.load());
				if(queue_complete(&client->txq_obj, (struct queue_entry *)nqe, QUEUE_NONBLOCK)) {
					fprintf(stderr, "WARNING - completion would have blocked\n");
				}
			}
		} else {
			/* got a completion for one of our commands */
			handle_completion(client, nqe);
		}
	}

	void handle_handler_queue(struct handler_queue_entry *hqe)
	{
		switch(hqe->cmd) {
			case HANDLER_QUEUE_ADD_CLIENT: {
				std::lock_guard<std::mutex> _lg(lock);
				for(auto client : adding_clients) {
					fprintf(stderr, "added client: %s\n", client->name.c_str());
					clients.push_back(client);
				}
				adding_clients.clear();
				cv.notify_all();
			} break;
			case HANDLER_QUEUE_DEL_CLIENT: {
				std::shared_ptr<net_client> client;
				{
					std::lock_guard<std::mutex> _lg(lock);
					client = clients[hqe->client_idx];
					fprintf(stderr, "erase %ld / %ld\n", hqe->client_idx, clients.size());
					clients.erase(clients.begin() + hqe->client_idx);
					queue_complete(&client_add_queue, (struct queue_entry *)hqe, 0);
				}
				struct nstack_queue_entry nqe;
				while(queue_receive(&client->txq_obj, (struct queue_entry *)&nqe, QUEUE_NONBLOCK)
				      == 0) {
					handle_client(client, &nqe, true, true);
				}
				while(
				  queue_get_finished(&client->rxq_obj, (struct queue_entry *)&nqe, QUEUE_NONBLOCK)
				  == 0) {
					handle_client(client, &nqe, true, false);
				}
				fprintf(stderr, "removed client: %s\n", client->name.c_str());
			} break;
		}
		qspec_build();
	}

	void aaaa()
	{
		while(1) {
			while(0) {
				usleep(100);
			}
			struct nstack_queue_entry nqe;
			queue_get_finished(&clients[0]->rxq_obj, (struct queue_entry *)&nqe, 0);
			handle_client(clients[0], &nqe, false, false);
		}
	}

	void handler_thread()
	{
		kso_set_name(NULL, "net_client_handler");
		for(;;) {
			// fprintf(stderr, "qe\n");
			ssize_t ret = queue_sub_dequeue_multiple(qspec_len, qspec);
			// fprintf(stderr, "qDONE\n");
			// fprintf(stderr, "ret: %ld\n", ret);
			/* TODO: check ret */
			(void)ret;
			for(size_t i = 1; i < qspec_len; i++) {
				if(qspec[i].ret != 0) {
					/* got a message */
					size_t client_idx = (i - 1) / 2;
					bool is_tx = !!(i % 2);
					//	fprintf(stderr, "got a message on queue %ld: %ld %d\n", i, client_idx,
					// is_tx);
					std::shared_ptr<net_client> client;
					{
						std::lock_guard<std::mutex> _lg(lock);
						client = clients[client_idx];
					}
					handle_client(
					  client, (struct nstack_queue_entry *)qspec[i].result, false, is_tx);
				}
			}
			if(qspec[0].ret != 0) {
				handle_handler_queue((struct handler_queue_entry *)qspec[0].result);
#if 0
				fprintf(stderr, "SWITCHING\n");
				std::thread ttt = std::thread(&handler::aaaa, this);
				while(1) {
					struct nstack_queue_entry nqe;
					queue_receive(&clients[0]->txq_obj, (struct queue_entry *)&nqe, 0);

					handle_client(clients[0], &nqe, false, true);
				}
#endif
			}
		}
	}
};

static std::vector<handler *> handlers;
static std::mutex handlers_lock;

int net_client::init_objects()
{
	/* TODO: failure cleanup; tie to view (?) */
	int r;
	if((r = twz_object_new(&txq_obj,
	      NULL,
	      NULL,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE | TWZ_OC_TIED_NONE))) {
		return r;
	}

	if((r = twz_object_new(&txbuf_obj,
	      NULL,
	      NULL,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE | TWZ_OC_TIED_NONE))) {
		return r;
	}

	if((r = twz_object_new(&rxq_obj,
	      NULL,
	      NULL,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE | TWZ_OC_TIED_NONE))) {
		return r;
	}

	if((r = twz_object_new(&rxbuf_obj,
	      NULL,
	      NULL,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE | TWZ_OC_TIED_NONE))) {
		return r;
	}

	if((r = queue_init_hdr(&txq_obj,
	      12,
	      sizeof(struct nstack_queue_entry),
	      12,
	      sizeof(struct nstack_queue_entry)))) {
		return r;
	}

	if((r = queue_init_hdr(&rxq_obj,
	      12,
	      sizeof(struct nstack_queue_entry),
	      12,
	      sizeof(struct nstack_queue_entry)))) {
		return r;
	}

	return 0;
}

extern "C" {
DECLARE_SAPI_ENTRY(open_client,
  NSTACK_GATE_OPEN_CLIENT,
  int,
  int flags,
  const char *name,
  struct nstack_open_ret *ret)
{
	(void)flags;
	std::shared_ptr<net_client> client = std::make_shared<net_client>(flags, name);
	fprintf(stderr, "open client : %p\n", client.get());
	int r = twz_object_init_guid(&client->thrdobj, twz_thread_repr_base()->reprid, FE_READ);
	if(r) {
		return r;
	}
	r = twz_object_wire(NULL, &client->thrdobj);
	if(r) {
		return r;
	}
	r = client->init_objects();
	if(r) {
		return r;
	}

	ret->txq_id = twz_object_guid(&client->txq_obj);
	ret->rxq_id = twz_object_guid(&client->rxq_obj);
	ret->txbuf_id = twz_object_guid(&client->txbuf_obj);
	ret->rxbuf_id = twz_object_guid(&client->rxbuf_obj);
	{
		std::lock_guard<std::mutex> _lg(handlers_lock);
		handler *handler = handlers[0]; // TODO: maybe we can have more than 1 handler?
		handler->add_client(client);
	}
	return 0;
}
}

int client_handlers_init()
{
	twzobj api_obj;
	int r = twz_object_new(&api_obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE);
	if(r) {
		return r;
	}
	twz_secure_api_create(&api_obj, "networking-stack");

	auto h = new handler();
	handlers.push_back(h);

	twz_name_assign(twz_object_guid(&api_obj), "/dev/nstack");
	return 0;
}
