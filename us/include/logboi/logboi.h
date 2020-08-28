#pragma once

#include <twz/gate.h>
#include <twz/obj.h>
#include <twz/security.h>

#define LOGBOI_GATE_OPEN 1
int logboi_open_connection(struct secure_api_header *hdr, objid_t *_id)
{
	size_t ctx = 0;
	void *id = twz_secure_api_alloc_stackarg(sizeof(*_id), &ctx);
	int r = twz_secure_api_call1(hdr, LOGBOI_GATE_OPEN, id);
	*_id = *(objid_t *)id;
	return r;
}
