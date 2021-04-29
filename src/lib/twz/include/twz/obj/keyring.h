#pragma once

#include <twz/objid.h>
#include <twz/sys/key.h>

struct __twzobj;
typedef struct __twzobj twzobj;

struct keyring_hdr {
	struct key_hdr *dfl_pubkey;
	struct key_hdr *dfl_prikey;
};
