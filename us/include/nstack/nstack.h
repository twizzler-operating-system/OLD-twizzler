#pragma once

#include <twz/gate.h>
#include <twz/queue.h>

#define NSTACK_GATE_OPEN_CLIENT 1

#ifdef __cplusplus
extern "C" {
#endif

#include <nstack/_types.h>

enum {
	/* these are commands that the client can send to the networking stack */
	NSTACK_CMD_CONNECT, /* create a persistent connection, connecting to an end point */
	NSTACK_CMD_BIND,    /* create a persistent accepting connection */
	NSTACK_CMD_ACCEPT,  /* accept a new connection on a persistent accepting connection, creating a
	                       new persistent connection */
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
	uint16_t cmd;
	/* flags contain any flags that modify the operation of the command */
	uint16_t flags;
	/* the networking stack will maintain a set of persistent connections, and this ID will specify
	 * which one is it. The NSTACK_CMD_CONNECT, NSTACK_CMD_BIND, and NSTACK_CMD_ACCEPT commands
	 * return persistent connection IDs. */
	uint16_t connid;
	uint16_t _resv16;
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

#ifdef __cplusplus
	nstack_queue_entry(nstack_queue_entry &other)
	{
		qe.info = other.qe.info;
		qe.cmd_id = other.qe.cmd_id.load();
		cmd = other.cmd;
		flags = other.flags;
		connid = other.connid;
		addr = other.addr;
		data_ptr = other.data_ptr;
		data_len = other.data_len;
	}

	nstack_queue_entry() = default;
#endif
};

struct nstack_open_ret {
	objid_t txq_id, rxq_id;
	objid_t txbuf_id, rxbuf_id;
};

/* Clients can call this, and it will jump to the networking stack and perform the client setup */
static inline int nstack_open_client(struct secure_api *api,
  int flags,
  const char *_name,
  struct nstack_open_ret *_ret)
{
	size_t ctx = 0;
	void *ret = twz_secure_api_alloc_stackarg(sizeof(struct nstack_open_ret), &ctx);
	char *name = (char *)twz_secure_api_alloc_stackarg(strlen(_name) + 1, &ctx);
	strcpy(name, _name);
	int r = twz_secure_api_call3(api, NSTACK_GATE_OPEN_CLIENT, flags, name, ret);
	*_ret = *(struct nstack_open_ret *)ret;
	return r;
}

#ifdef __cplusplus
}
#endif
