#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <twix/twix.h>
#include <twz/obj/event.h>

#include "state.h"

class async_job
{
  public:
	async_job(std::shared_ptr<queue_client> _client,
	  twix_queue_entry &_tqe,
	  int (*_poll)(async_job &, struct event *),
	  void (*_callback)(async_job &, int),
	  void *_data,
	  uint64_t _type)
	{
		client = _client;
		tqe = _tqe;
		data = _data;
		poll = _poll;
		callback = _callback;
		type = _type;
	}

	std::shared_ptr<queue_client> client;
	twix_queue_entry tqe;
	void *data;
	uint64_t type;

	int (*poll)(async_job &, struct event *ev);
	void (*callback)(async_job &, int);
};

class async_handler
{
	std::vector<async_job> jobs;
	std::thread thread;

	std::mutex lock;
	std::vector<async_job> incoming_jobs;

	std::atomic_uint_least64_t waker;

  public:
	async_handler();

	int add_job(async_job job);

	void async_thread();
};

void async_callback_complete(async_job &job, int ret);
void async_add_job(async_job job);
void async_init();
