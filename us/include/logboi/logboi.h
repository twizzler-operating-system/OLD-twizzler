#pragma once

#include <twz/gate.h>
#include <twz/obj.h>
#include <twz/security.h>

#include <string.h>

#define LOGBOI_GATE_OPEN 1
int logboi_open_connection(struct secure_api_header *hdr,
  const char *_name,
  int flags,
  objid_t *_id)
{
	size_t ctx = 0;
	void *id = twz_secure_api_alloc_stackarg(sizeof(*_id), &ctx);
	char *name = (char *)twz_secure_api_alloc_stackarg(strlen(_name) + 1, &ctx);
	printf("DOING CALL: %p\n", name);
	strcpy(name, _name);
	int r = twz_secure_api_call3(hdr, LOGBOI_GATE_OPEN, name, flags, id);
	*_id = *(objid_t *)id;
	return r;
}
