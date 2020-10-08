#pragma once

#include <stdint.h>
#include <twz/_objid.h>

struct keyring_hdr;
struct user_hdr {
	objid_t dfl_secctx;
	struct keyring_hdr *kr;
	char *name;
	uint64_t flags;
};
