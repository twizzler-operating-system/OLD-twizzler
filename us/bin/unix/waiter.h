#pragma once

#include <atomic>
#include <time.h>
#include <twz/thread.h>

class generic_wait_task
{
  public:
	queue_client *client;
	struct sys_thread_sync_args tsa;
	struct timespec timeout;
	struct twix_queue_entry tqe;

	generic_wait_task(atomic_ulong *word, atomic_ulong target, struct timespec *spec)
	{
		timeout = *spec;
		twz_thread_sync_init(&tsa, THREAD_SYNC_SLEEP, word, target);
	}

	virtual void callback()
	{
	}
};
