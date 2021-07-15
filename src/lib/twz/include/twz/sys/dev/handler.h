#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#ifdef __cplusplus
#include <atomic>
#else
#include <stdatomic.h>
#endif

struct device;
struct handler_thread;
struct interrupt_handler {
	void (*fn)(int nr, void *data);
	void *data;
	struct device *device;
	int nr;
	struct interrupt_handler *next;
	struct handler_thread *handler;
};

struct handler_thread {
	pthread_t thread;
	pthread_mutex_t lock;
	struct interrupt_handler *handlers;
#ifdef __cplusplus
	std::atomic_uint_least64_t ctl;
#else
	atomic_uint_least64_t ctl;
#endif
};

static struct interrupt_handler interrupt_handler_create(struct device *device,
  int nr,
  void (*fn)(int, void *),
  void *data)
{
	return (struct interrupt_handler){
		.fn = fn,
		.data = data,
		.device = device,
		.nr = nr,
	};
}

void handler_add_interrupt(struct handler_thread *handler, struct interrupt_handler *ih);

void handler_remove_interrupt(struct interrupt_handler *ih);

int handler_thread_create(struct handler_thread *handler);

#ifdef __cplusplus
}
#endif
