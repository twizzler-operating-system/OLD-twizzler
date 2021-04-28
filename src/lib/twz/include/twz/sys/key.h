#pragma once
#include <stdint.h>

#define TWZ_KEY_PRI 1

struct key_hdr {
	unsigned char *keydata;
	uint64_t keydatalen;
	uint32_t type;
	uint32_t flags;
};
