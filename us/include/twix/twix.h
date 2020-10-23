#pragma once

#include <twz/gate.h>
#include <twz/obj.h>
#include <twz/queue.h>
#include <twz/security.h>

#define TWIX_GATE_OPEN_QUEUE 1
int twix_open_queue(struct secure_api *api, int flags, objid_t *_id)
{
	size_t ctx = 0;
	void *id = twz_secure_api_alloc_stackarg(sizeof(*_id), &ctx);
	int r = twz_secure_api_call2(api->hdr, TWIX_GATE_OPEN_QUEUE, flags, id);
	*_id = *(objid_t *)id;
	return r;
}

struct twix_queue_entry {
	struct queue_entry qe;
	int x;
};
