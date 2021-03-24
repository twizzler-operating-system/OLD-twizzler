#pragma once

#include <twz/_key.h>
#include <twz/_objid.h>

struct keyring_hdr {
	struct key_hdr *dfl_pubkey;
	struct key_hdr *dfl_prikey;
};

int twz_keyring_lookup(twzobj *obj, unsigned char *fp, size_t fplen, objid_t *pub, objid_t *pri);

int twz_keyring_insert(twzobj *obj, unsigned char *fp, size_t fplen, objid_t pub, objid_t pri);
