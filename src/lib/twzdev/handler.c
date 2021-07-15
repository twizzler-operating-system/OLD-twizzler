#include "include/twz/sys/dev/device.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <twz/sys/dev/handler.h>
#include <twz/sys/sync.h>
#include <twz/sys/syscall.h>

void handler_add_interrupt(struct handler_thread *handler, struct interrupt_handler *ih)
{
	assert(ih->handler == NULL);
	pthread_mutex_lock(&handler->lock);
	ih->handler = handler;
	ih->next = handler->handlers;
	handler->handlers = ih;
	pthread_mutex_unlock(&handler->lock);
}

void handler_remove_interrupt(struct interrupt_handler *ih)
{
	assert(ih->handler != NULL);
	pthread_mutex_lock(&ih->handler->lock);
	struct interrupt_handler **handler = &ih->handler->handlers;
	while((*handler) != ih)
		handler = &(*handler)->next;
	*handler = ih->next;
	pthread_mutex_unlock(&ih->handler->lock);
}

static atomic_uint_least64_t *_handler_fn_get_syncpoint(struct interrupt_handler *ih)
{
	return &ih->device->hdr->interrupts[ih->nr].sp;
}

static int _handler_fn_check_interrupt(struct interrupt_handler *ih)
{
	atomic_uint_least64_t *sp = _handler_fn_get_syncpoint(ih);
	return !!atomic_exchange(sp, 0);
}

static void *_handler_fn(void *arg)
{
	struct handler_thread *ht = arg;
	size_t sleep_len = 32;
	struct sys_thread_sync_args *sleep_args =
	  calloc(sleep_len, sizeof(struct sys_thread_sync_args));
	twz_thread_sync_init(&sleep_args[0], THREAD_SYNC_SLEEP, &ht->ctl, 0);
	while(1) {
		size_t sleep_count = 1;
		int found = 0;
		pthread_mutex_lock(&ht->lock);
		/* TODO: try to do most of the setup work for sync points once, not every time */
		for(struct interrupt_handler *handler = ht->handlers; handler; handler = handler->next) {
			if(_handler_fn_check_interrupt(handler)) {
				if(handler->fn) {
					handler->fn(handler->nr, handler->data);
				}
				found = 1;
			}

			if(!found) {
				atomic_uint_least64_t *sp = _handler_fn_get_syncpoint(handler);
				if(sleep_count >= sleep_len) {
					sleep_len *= 2;
					sleep_args =
					  realloc(sleep_args, sleep_len * sizeof(struct sys_thread_sync_args));
				}
				twz_thread_sync_init(&sleep_args[sleep_count], THREAD_SYNC_SLEEP, sp, 0);
				sleep_count++;
			}
		}
		pthread_mutex_unlock(&ht->lock);

		if(!found) {
			if(twz_thread_sync_multiple(sleep_count, sleep_args, NULL)) {
				fprintf(stderr, "interrupt handler thread failed to sleep\n");
			}
		}
	}
}

int handler_thread_create(struct handler_thread *handler)
{
	handler->handlers = NULL;
	pthread_mutex_init(&handler->lock, NULL);
	handler->ctl = 0;
	return pthread_create(&handler->thread, NULL, _handler_fn, handler);
}
