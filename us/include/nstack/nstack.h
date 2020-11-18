#pragma once

#include <twz/queue.h>

#define NSTACK_GATE_OPEN_CLIENT 1

#ifdef __cplusplus
extern "C" {
#endif

/* TODO: figure out how we want to represent an address */
struct netaddr {
	int type;
	char addr[32];
};

enum {
	/* these are commands that the client can send to the networking stack */
	NSTACK_CMD_CONNECT,
	NSTACK_CMD_BIND,
	NSTACK_CMD_SEND,
	NSTACK_CMD_PUSH,

	/* these are commands that the networking stack will send back to the client */
	NSTACK_CMD_RECV,
	NSTACK_CMD_DROP,
};

struct nstack_queue_entry {
	/* the client and nstack use qe.info to coordinate which queue command this (for async
	 * completion) */
	struct queue_entry qe;
	/* the command is filled from the enum above */
	uint32_t cmd;
	/* flags contain any flags that modify the operation of the command */
	uint32_t flags;
	union {
		/* optional larger arguments, including return value (filled out for completion). We might
		 * consider having a simpler completion structure that just contains qe and ret. */
		int64_t ret;
		int64_t args[4];
		/* optional address */
		struct netaddr addr;
	};

	/* a persistent pointer to the packet data. */
	void *data_ptr;
	/* the length of the packet data (optional) */
	uint32_t data_len;
	/* reserved for struct padding */
	uint32_t _resv;
};

/* Clients can call this, and it will jump to the networking stack and perform the client setup */
static inline int nstack_open_client(struct secure_api *api,
  int flags,
  const char *_name,
  objid_t *_txq,
  objid_t *_txbuf,
  objid_t *_rxq,
  objid_t *_rxbuf)
{
	size_t ctx = 0;
	void *tqid = twz_secure_api_alloc_stackarg(sizeof(*_txq), &ctx);
	void *rqid = twz_secure_api_alloc_stackarg(sizeof(*_rxq), &ctx);
	void *tbid = twz_secure_api_alloc_stackarg(sizeof(*_txbuf), &ctx);
	void *rbid = twz_secure_api_alloc_stackarg(sizeof(*_rxbuf), &ctx);
	char *name = (char *)twz_secure_api_alloc_stackarg(strlen(_name) + 1, &ctx);
	int r =
	  twz_secure_api_call6(api->hdr, NSTACK_GATE_OPEN_CLIENT, flags, name, tqid, tbid, rqid, rbid);
	*_txq = *(objid_t *)tqid;
	*_txbuf = *(objid_t *)tbid;
	*_rxq = *(objid_t *)rqid;
	*_rxbuf = *(objid_t *)rbid;
	return r;
}

#ifdef __cplusplus
}
#endif
