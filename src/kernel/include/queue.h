#pragma once

#define __QUEUE_TYPES_ONLY
#include <twz/obj/_queue.h>
#include <twz/sys/syscall.h>

struct object;
int kernel_queue_submit(struct object *, struct queue_hdr *hdr, struct queue_entry *qe);
int kernel_queue_get_cmpls(struct object *obj, struct queue_hdr *hdr, struct queue_entry *qe);

int kernel_queue_assign(enum kernel_queues kq, struct object *obj);
int kernel_queue_pager_request_object(objid_t id);
int kernel_queue_pager_request_page(struct object *obj, size_t pg);
struct queue_hdr *kernel_queue_get_hdr(enum kernel_queues kq);
struct object *kernel_queue_get_object(enum kernel_queues kq);
