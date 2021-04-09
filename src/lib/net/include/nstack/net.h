#pragma once

#include <nstack/_types.h>
#include <nstack/nstack.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <twz/obj.h>

struct netreq;
struct netcon;
struct netmgr;

#define NETEVENT_FLAGS_CON 1

struct netevent {
	int event;
	int info;
	int pad;
	int flags;
	void *data_ptr;
	size_t data_len;
	struct nstack_queue_entry nqe;
	union {
		struct netmgr *mgr;
		struct netcon *con;
	};
	struct netevent *next, *prev;
};

struct netmgr {
	twzobj txq_obj, rxq_obj;
	twzobj txbuf_obj, rxbuf_obj;
	pthread_mutex_t lock;
	uint32_t *idlist;
	uint32_t maxid, idlistend, idlistlen;

	struct netreq *outstanding;
	struct netcon *conlist;

	pthread_t worker; // TODO get rid of this

	pthread_cond_t event_cv;
	struct netevent eventlist_sentry;
};

enum {
	NETEVENT_RX = 1,
	NETEVENT_ACCEPT,
	NETEVENT_SHUTDOWN,
};

#define NETEVENT_SHUTDOWN_READ 1
#define NETEVENT_SHUTDOWN_WRITE 2
#define NETEVENT_SHUTDOWN_RESET 4

#define NETEVENT_RX 1
#define NETEVENT_ACCEPT 2

enum netcon_state {
	NETCON_STATE_NONE,
	NETCON_STATE_CONNECTED,
	NETCON_STATE_SHUTDOWN,
};

#define NETCON_FLAGS_READEOF 1
#define NETCON_FLAGS_WRITEEOF 2

struct netcon {
	struct netmgr *mgr;
	uint16_t id;
	uint16_t flags;
	enum netcon_state state;

	struct netcon *next;

	pthread_mutex_t lock;
	pthread_cond_t event_cv;
	struct netevent eventlist_sentry;
};

struct pbuf {
	struct pbuf *next;
	char data[];
};

struct netbuf {
	void *ptr;
	size_t len;
	struct pbuf *_pb;
};

struct netmgr *netmgr_create(const char *name, int flags);
void netmgr_destroy(struct netmgr *mgr);

struct timespec;
struct netcon *netmgr_connect(struct netmgr *mgr,
  struct netaddr *addr,
  int flags,
  struct timespec *timeout);
int netmgr_bind(struct netmgr *mgr, struct netaddr *addr, int flags);

ssize_t netcon_send(struct netcon *con, const void *buf, size_t len, int flags);
ssize_t netcon_recv(struct netcon *con, void *buf, size_t len, int flags);
struct netcon *netcon_accept(struct netmgr *mgr);

int netcon_transmit_buf(struct netcon *con, struct netbuf *buf);
int netmgr_get_buf(struct netmgr *mgr, struct netbuf *buf);

#define NETCON_SHUTDOWN_READ 1
#define NETCON_SHUTDOWN_WRITE 2

void netcon_shutdown(struct netcon *con, int flags);
void netcon_destroy(struct netcon *con);

#define NETEVENT_WAIT_NONBLOCK 1

struct netevent *netevent_next_mgr(struct netmgr *mgr, int);
struct netevent *netevent_next_con(struct netcon *con, int);
void netevent_done(struct netevent *ev);

struct pbuf;

size_t pbuf_datalen(twzobj *bufobj);
void pbuf_release(twzobj *bufobj, struct pbuf *buf);
struct pbuf *pbuf_alloc(twzobj *bufobj);
void pbuf_init(twzobj *bufobj, size_t datalen);

struct netaddr netaddr_from_ipv4_string(const char *str, uint16_t port);

/* TESTING */
void netmgr_echo(struct netmgr *);
void netmgr_wait_all_tx_complete(struct netmgr *mgr);
