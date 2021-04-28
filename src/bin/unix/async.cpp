#include "async.h"

int async_handler::add_job(async_job job)
{
	std::lock_guard _lg(lock);
	incoming_jobs.push_back(job);
	event_wake_other(&waker, 1, 1);
	return 0;
}

static std::vector<async_handler *> async_handlers;

void async_init()
{
	async_handlers.push_back(new async_handler());
}

void async_add_job(async_job job)
{
	async_handlers[0]->add_job(job);
}

async_handler::async_handler()
{
	thread = std::thread(&async_handler::async_thread, this);
}

void async_callback_complete(async_job &job, int ret)
{
	job.tqe.ret = ret;
	job.client->complete(&job.tqe);
}

#include <twz/sys/thread.h>

void async_handler::async_thread()
{
	twz_thread_set_name("twix-server-async");
	for(;;) {
		{
			/* collect new jobs */
			std::lock_guard<std::mutex> _lg(lock);
			event_clear_other(&waker, 1);
			while(!incoming_jobs.empty()) {
				auto j = incoming_jobs.back();
				incoming_jobs.pop_back();
				jobs.push_back(j);
			}
		}

		/* collect events */
		struct event *events = (struct event *)calloc(jobs.size() + 1, sizeof(struct event));
		event_init_other(&events[0], &waker, 1);
		size_t sleep_count = 1;
		for(size_t i = 0; i < jobs.size(); i++) {
			struct event ev;
			int ret = jobs[i].poll(jobs[i], &ev);
			if(ret < 0) {
				jobs[i].callback(jobs[i], ret);
				jobs.erase(jobs.begin() + i);
			} else if(ret > 0) {
				jobs[i].callback(jobs[i], 0);
				jobs.erase(jobs.begin() + i);
			} else {
				events[sleep_count++] = ev;
			}
		}
		/* TODO: handle errors, and max waits... */
		event_wait(sleep_count, events, NULL);
		free(events);
	}
}
