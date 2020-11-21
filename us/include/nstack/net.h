#pragma once

#include <nstack/_types.h>
#include <pthread.h>
#include <stdint.h>
#include <twz/obj.h>

struct netreq;
struct netmgr {
	twzobj txq_obj, rxq_obj;
	twzobj txbuf_obj, rxbuf_obj;
	pthread_mutex_t lock;
	uint32_t *idlist;
	uint32_t maxid, idlistend, idlistlen;

	struct netreq *outstanding;
};

struct netmgr *netmgr_create(const char *name, int flags);
void netmgr_destroy(struct netmgr *mgr);
void pbuf_release(twzobj *bufobj, struct pbuf *buf);
struct pbuf *pbuf_alloc(twzobj *bufobj);

/* TESTING */
void netmgr_echo(struct netmgr *);
void netmgr_wait_all_tx_complete(struct netmgr *mgr);
