#pragma once

#include <nstack/_types.h>
#include <pthread.h>
#include <stdint.h>
#include <twz/obj.h>

struct netreq;
struct netcon;
struct netmgr {
	twzobj txq_obj, rxq_obj;
	twzobj txbuf_obj, rxbuf_obj;
	pthread_mutex_t lock;
	uint32_t *idlist;
	uint32_t maxid, idlistend, idlistlen;

	struct netreq *outstanding;
	struct netcon *conlist;

	pthread_t worker; // TODO get rid of this
};

#define NETEVENT_RX 1

#include <nstack/nstack.h>

struct netevent {
	int event;
	int flags;
	void *data_ptr;
	size_t data_len;
	struct nstack_queue_entry nqe;
	struct netevent *next, *prev;
};

struct netcon {
	struct netmgr *mgr;
	uint16_t id;

	struct netcon *next;

	pthread_mutex_t lock;
	pthread_cond_t event_cv;
	struct netevent eventlist_sentry;
};

struct pbuf {
	struct pbuf *next;
	char data[];
};

struct netmgr *netmgr_create(const char *name, int flags);
void netmgr_destroy(struct netmgr *mgr);

struct timespec;
struct netcon *netmgr_connect(struct netmgr *mgr,
  struct netaddr *addr,
  int flags,
  struct timespec *timeout);

ssize_t netcon_send(struct netcon *con, const void *buf, size_t len, int flags);

struct pbuf;

size_t pbuf_datalen(twzobj *bufobj);
void pbuf_release(twzobj *bufobj, struct pbuf *buf);
struct pbuf *pbuf_alloc(twzobj *bufobj);
void pbuf_init(twzobj *bufobj, size_t datalen);

/* TESTING */
void netmgr_echo(struct netmgr *);
void netmgr_wait_all_tx_complete(struct netmgr *mgr);
