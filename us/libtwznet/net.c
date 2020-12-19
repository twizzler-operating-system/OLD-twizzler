#include <errno.h>
#include <nstack/net.h>
#include <nstack/nstack.h>
#include <stdio.h>
#include <stdlib.h>
static struct netcon *__create_netcon(struct netmgr *mgr, uint16_t id);
static __inline__ unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

struct netreq {
	struct nstack_queue_entry nqe;
	void (*callback)(struct netreq *req, struct nstack_queue_entry *nqe);
	void *data;
	struct netreq *next;
};

struct netevent *netevent_create(struct netmgr *mgr,
  struct nstack_queue_entry *nqe,
  int event,
  int info,
  int flags,
  void *ptr,
  size_t len)
{
	struct netevent *ev = malloc(sizeof(*ev));
	ev->event = event;
	ev->mgr = mgr;
	ev->flags = flags;
	ev->info = info;
	ev->data_ptr = ptr;
	ev->data_len = len;
	ev->nqe = *nqe;
	return ev;
}

static void __netevent_append(struct netevent *sentry, struct netevent *ne)
{
	ne->next = sentry;
	ne->prev = sentry->prev;
	sentry->prev->next = ne;
	sentry->prev = ne;
}

static void __netevent_remove(struct netevent *ev)
{
	ev->prev->next = ev->next;
	ev->next->prev = ev->prev;
	ev->next = ev->prev = NULL;
}

#include <assert.h>
void netevent_destroy(struct netevent *ev)
{
	assert(ev->next == NULL);
	queue_complete(&ev->mgr->rxq_obj, (struct queue_entry *)&ev->nqe, 0);
	free(ev);
}

static uint32_t alloc_id(struct netmgr *mgr)
{
	pthread_mutex_lock(&mgr->lock);
	if(mgr->idlistend > 0) {
		uint32_t ret = mgr->idlist[mgr->idlistend - 1];
		mgr->idlistend--;
		pthread_mutex_unlock(&mgr->lock);
		return ret;
	}
	uint32_t ret = mgr->maxid++;
	pthread_mutex_unlock(&mgr->lock);
	return ret;
}

static void release_id(struct netmgr *mgr, uint32_t id)
{
	pthread_mutex_lock(&mgr->lock);

	if(mgr->idlistend < mgr->idlistlen) {
		mgr->idlist[mgr->idlistend++] = id;
		pthread_mutex_unlock(&mgr->lock);
		return;
	}

	mgr->idlistlen *= 2;
	mgr->idlist = realloc(mgr->idlist, mgr->idlistlen * sizeof(uint32_t));
	mgr->idlist[mgr->idlistend++] = id;
	pthread_mutex_unlock(&mgr->lock);
}

static void add_outstanding_tx(struct netmgr *mgr, struct netreq *req)
{
	uint32_t id = alloc_id(mgr);
	// fprintf(stderr, "add_out: %d\n", id);
	req->nqe.qe.info = id;
	pthread_mutex_lock(&mgr->lock);

	req->next = mgr->outstanding;
	mgr->outstanding = req;

	pthread_mutex_unlock(&mgr->lock);
}

static void handle_completion_from_tx(struct netmgr *mgr, struct nstack_queue_entry *nqe)
{
	// fprintf(stderr, "got compl: %d %x\n", nqe->qe.info, nqe->qe.cmd_id);
	pthread_mutex_lock(&mgr->lock);
	struct netreq *req, *prev = NULL;
	for(req = mgr->outstanding; req; (prev = req), req = req->next) {
		if(req->nqe.qe.info == nqe->qe.info) {
			if(prev)
				prev->next = req->next;
			else
				mgr->outstanding = req->next;
			pthread_mutex_unlock(&mgr->lock);
			if(req->callback) {
				req->callback(req, nqe);
			}
			release_id(mgr, req->nqe.qe.info);
			free(req);
			return;
		}
	}

	usleep(100);
	fprintf(stderr, "could not find outstanding request for ID %d\n", nqe->qe.info);

	for(req = mgr->outstanding; req; (prev = req), req = req->next) {
		fprintf(stderr, "  have %d\n", req->nqe.qe.info);
	}
	usleep(100000);
	abort();
	pthread_mutex_unlock(&mgr->lock);
}

struct netcon *netmgr_lookup_netcon(struct netmgr *mgr, uint16_t id)
{
	pthread_mutex_lock(&mgr->lock);
	for(struct netcon *con = mgr->conlist; con; con = con->next) {
		if(con->id == id) {
			pthread_mutex_unlock(&mgr->lock);
			return con;
		}
	}

	pthread_mutex_unlock(&mgr->lock);
	return NULL;
}

static void handle_cmd(struct netmgr *mgr, struct nstack_queue_entry *nqe)
{
	bool complete = true;
	switch(nqe->cmd) {
		case NSTACK_CMD_RECV: {
			struct netcon *con = netmgr_lookup_netcon(mgr, nqe->connid);
			struct netevent *ev = netevent_create(mgr,
			  nqe,
			  NETEVENT_RX,
			  0,
			  0,
			  twz_object_lea(&mgr->rxq_obj, nqe->data_ptr),
			  nqe->data_len);
			pthread_mutex_lock(&con->lock);
			__netevent_append(&con->eventlist_sentry, ev);
			pthread_cond_signal(&con->event_cv);
			pthread_mutex_unlock(&con->lock);
			complete = false;
		} break;
		case NSTACK_CMD_ACCEPT: {
			__create_netcon(mgr, nqe->connid);
			struct netevent *ev =
			  netevent_create(mgr, nqe, NETEVENT_ACCEPT, nqe->connid, 0, NULL, 0);
			pthread_mutex_lock(&mgr->lock);
			__netevent_append(&mgr->eventlist_sentry, ev);
			pthread_cond_signal(&mgr->event_cv);
			pthread_mutex_unlock(&mgr->lock);
			complete = false;
		} break;
		default:
			fprintf(stderr, "[net] unknown command from nstack %d\n", nqe->cmd);
	}
	if(complete) {
		queue_complete(&mgr->rxq_obj, (struct queue_entry *)nqe, 0);
	}
}

#define RX_NONBLOCK 1
#define RX_LIMIT 2
#define RX_ATLEASTONE 4

#define THRESH 64

static void get_rx_commands(struct netmgr *mgr, int flags)
{
	struct nstack_queue_entry nqe;
	size_t i = 0;
	bool non_block = !!(flags & RX_NONBLOCK);
	/* TODO: we're assuming that we're single threaded here */
	while(1) {
		// pthread_mutex_lock(&mgr->lock);
		// fprintf(stderr, "RQ\n");
		if(queue_receive(&mgr->rxq_obj, (struct queue_entry *)&nqe, non_block ? QUEUE_NONBLOCK : 0)
		   == 0) {
		} else if(!non_block) {
			//	pthread_mutex_unlock(&mgr->lock);
			return;
		}
		// fprintf(stderr, "RDONE\n");
		// pthread_mutex_unlock(&mgr->lock);
		handle_cmd(mgr, &nqe);
		// fprintf(stderr, "HCDONE\n");
		if(flags & RX_ATLEASTONE) {
			return;
		}
		if((i++ > THRESH) && (flags & RX_LIMIT)) {
			return;
		}
	}
}

static void submit_tx(struct netmgr *mgr,
  struct nstack_queue_entry *nqe,
  void (*callback)(struct netreq *, struct nstack_queue_entry *),
  void *data)
{
	// long a, b;
	// a = rdtsc();
	struct netreq *req = malloc(sizeof(*req));
	req->callback = callback;
	req->data = data;
	req->nqe = *nqe;
	// b = rdtsc();
	// printf("stx init: %ld\n", b - a);
	// a = rdtsc();
	add_outstanding_tx(mgr, req);
	// b = rdtsc();
	// printf("stx outs: %ld\n", b - a);
	// a = rdtsc();
	queue_submit(&mgr->txq_obj, (struct queue_entry *)&req->nqe, 0);
	// b = rdtsc();
	// printf("stx qsub: %ld\n", b - a);
}

static void complete_tx(struct netmgr *mgr)
{
	struct nstack_queue_entry nqe;
	while(1) {
		pthread_mutex_lock(&mgr->lock);
		if(!mgr->outstanding) {
			pthread_mutex_unlock(&mgr->lock);
			break;
		}
		int r = queue_get_finished(&mgr->txq_obj, (struct queue_entry *)&nqe, QUEUE_NONBLOCK);
		pthread_mutex_unlock(&mgr->lock);
		if(r == 0) {
			handle_completion_from_tx(mgr, &nqe);
		} else {
			break;
		}
	}
}

void netmgr_housekeeping(struct netmgr *mgr)
{
	complete_tx(mgr);
}

static void __echo_callback(struct netreq *req, struct nstack_queue_entry *nqe)
{
	fprintf(stderr, ":: echo callback %p\n", req->data);
}

void netmgr_echo(struct netmgr *mgr)
{
	struct nstack_queue_entry nqe = {};
	submit_tx(mgr, &nqe, __echo_callback, (void *)1234);

	complete_tx(mgr);
}

void netmgr_wait_all_tx_complete_until(struct netmgr *mgr, bool (*pred)(void *), void *data)
{
	struct nstack_queue_entry nqe;
	while(1) {
		pthread_mutex_lock(&mgr->lock);

		if(pred(data)) {
			pthread_mutex_unlock(&mgr->lock);
			break;
		}

		if(!mgr->outstanding) {
			pthread_mutex_unlock(&mgr->lock);
			break;
		}
		queue_get_finished(&mgr->txq_obj, (struct queue_entry *)&nqe, 0);
		pthread_mutex_unlock(&mgr->lock);
		handle_completion_from_tx(mgr, &nqe);
	}
}

void netmgr_wait_all_tx_complete(struct netmgr *mgr)
{
	struct nstack_queue_entry nqe;
	while(1) {
		pthread_mutex_lock(&mgr->lock);
		if(!mgr->outstanding) {
			pthread_mutex_unlock(&mgr->lock);
			break;
		}
		queue_get_finished(&mgr->txq_obj, (struct queue_entry *)&nqe, 0);
		pthread_mutex_unlock(&mgr->lock);
		handle_completion_from_tx(mgr, &nqe);
	}
}

static void *__netmgr_worker_main(void *data)
{
	struct netmgr *mgr = data;
	while(1) {
		get_rx_commands(mgr, 0);
	}
	return NULL;
}

struct netmgr *netmgr_create(const char *name, int flags)
{
	struct secure_api api;
	if(twz_secure_api_open_name("/dev/nstack", &api)) {
		return NULL;
	}

	struct nstack_open_ret ret;
	int r = nstack_open_client(&api, flags, name, &ret);
	if(r) {
		return NULL;
	}

	struct netmgr *mgr = malloc(sizeof(*mgr));

	if(twz_object_init_guid(&mgr->txq_obj, ret.txq_id, FE_READ | FE_WRITE))
		return NULL;
	if(twz_object_init_guid(&mgr->rxq_obj, ret.rxq_id, FE_READ | FE_WRITE))
		return NULL;
	if(twz_object_init_guid(&mgr->txbuf_obj, ret.txbuf_id, FE_READ | FE_WRITE))
		return NULL;
	if(twz_object_init_guid(&mgr->rxbuf_obj, ret.rxbuf_id, FE_READ | FE_WRITE))
		return NULL;

	pthread_mutex_init(&mgr->lock, NULL);

	mgr->maxid = 0;
	mgr->idlistlen = 32;
	mgr->idlist = calloc(mgr->idlistlen, sizeof(uint32_t));
	mgr->idlistend = 0;

	mgr->outstanding = NULL;

	mgr->conlist = NULL;

	pbuf_init(&mgr->txbuf_obj, 1000 /* TODO: MTU */);

	pthread_create(&mgr->worker, NULL, __netmgr_worker_main, mgr);

	mgr->eventlist_sentry.next = &mgr->eventlist_sentry;
	mgr->eventlist_sentry.prev = &mgr->eventlist_sentry;
	pthread_cond_init(&mgr->event_cv, NULL);

	return mgr;
}

void netmgr_destroy(struct netmgr *mgr)
{
	twz_object_release(&mgr->txq_obj);
	twz_object_release(&mgr->rxq_obj);
	twz_object_release(&mgr->txbuf_obj);
	twz_object_release(&mgr->rxbuf_obj);

	pthread_mutex_destroy(&mgr->lock);

	free(mgr->idlist);
	free(mgr);
}

struct waiter_callback_info {
	pthread_mutex_t lock;
	struct netmgr *mgr;
	struct pbuf *pbuf;
	long ret;
	uint16_t connid;
	int flags;
	bool done;
};

#define WAITER_INFO_ALLOC 1
#define WAITER_INFO_AUTODESTROY 2

static inline void waiter_callback_init(struct waiter_callback_info *info,
  struct netmgr *mgr,
  int flags)
{
	pthread_mutex_init(&info->lock, NULL);
	info->ret = 0;
	info->done = false;
	info->pbuf = NULL;
	info->flags = flags;
	info->mgr = mgr;
}

static inline struct waiter_callback_info *waiter_callback_create(struct netmgr *mgr, int flags)
{
	struct waiter_callback_info *info = malloc(sizeof(*info));
	waiter_callback_init(info, mgr, flags | WAITER_INFO_ALLOC);
	return info;
}

static inline void waiter_callback_destroy(struct waiter_callback_info *info)
{
	pthread_mutex_destroy(&info->lock);
	if(info->pbuf) {
		pbuf_release(&info->mgr->txbuf_obj, info->pbuf);
	}
	if(info->flags & WAITER_INFO_ALLOC) {
		free(info);
	}
}

static void __waiter_callback(struct netreq *req, struct nstack_queue_entry *nqe)
{
	struct waiter_callback_info *info = req->data;
	pthread_mutex_lock(&info->lock);
	info->done = true;
	info->ret = nqe->ret;
	info->connid = nqe->connid;
	pthread_mutex_unlock(&info->lock);
	if(info->flags & WAITER_INFO_AUTODESTROY) {
		waiter_callback_destroy(info);
	}
}

static bool __waiter_pred(void *data)
{
	struct waiter_callback_info *info = data;
	pthread_mutex_lock(&info->lock);
	bool ret = info->done;
	pthread_mutex_unlock(&info->lock);
	return ret;
}

static struct nstack_queue_entry nqe_build_connect(struct netaddr *addr, int flags)
{
	struct nstack_queue_entry nqe = {
		.cmd = NSTACK_CMD_CONNECT,
		.flags = flags,
		.addr = *addr,
	};
	return nqe;
}

static struct nstack_queue_entry nqe_build_bind(struct netaddr *addr, int flags)
{
	struct nstack_queue_entry nqe = {
		.cmd = NSTACK_CMD_BIND,
		.flags = flags,
		.addr = *addr,
	};
	return nqe;
}
static struct netcon *__create_netcon(struct netmgr *mgr, uint16_t id)
{
	struct netcon *con = malloc(sizeof(*con));
	con->id = id;
	con->mgr = mgr;
	con->eventlist_sentry.next = &con->eventlist_sentry;
	con->eventlist_sentry.prev = &con->eventlist_sentry;
	pthread_mutex_init(&con->lock, NULL);
	pthread_cond_init(&con->event_cv, NULL);
	pthread_mutex_lock(&mgr->lock);
	con->next = mgr->conlist;
	mgr->conlist = con;
	pthread_mutex_unlock(&mgr->lock);
	return con;
}

struct netcon *netmgr_connect(struct netmgr *mgr,
  struct netaddr *addr,
  int flags,
  struct timespec *timeout)
{
	struct waiter_callback_info info;
	struct nstack_queue_entry nqe = nqe_build_connect(addr, flags);
	waiter_callback_init(&info, mgr, 0);
	submit_tx(mgr, &nqe, __waiter_callback, &info);
	netmgr_wait_all_tx_complete_until(mgr, __waiter_pred, &info);

	if(info.ret != 0) {
		errno = info.ret;
		return NULL;
	}

	return __create_netcon(mgr, info.connid);
}

int netmgr_bind(struct netmgr *mgr, struct netaddr *addr, int flags)
{
	struct waiter_callback_info info;
	struct nstack_queue_entry nqe = nqe_build_bind(addr, flags);
	waiter_callback_init(&info, mgr, 0);
	submit_tx(mgr, &nqe, __waiter_callback, &info);
	netmgr_wait_all_tx_complete_until(mgr, __waiter_pred, &info);
	return info.ret;
}

static struct nstack_queue_entry nqe_build_send(struct netcon *con, void *vptr, size_t len)
{
	struct nstack_queue_entry nqe = {
		.cmd = NSTACK_CMD_SEND,
		.flags = 0,
		.connid = con->id,
		.data_ptr = twz_ptr_swizzle(&con->mgr->txq_obj, vptr, FE_READ),
		.data_len = len,
	};
	return nqe;
}

ssize_t netcon_send(struct netcon *con, const void *buf, size_t len, int flags)
{
	size_t count = 0;
	size_t buflen = pbuf_datalen(&con->mgr->txbuf_obj);
	long a, b;
	// a = rdtsc();
	complete_tx(con->mgr);
	// b = rdtsc();
	// printf("cmpl: %ld\n", b - a); //SLOW
	/* TODO: check connection state */
	while(count < len) {
		// a = rdtsc();
		struct pbuf *pb = pbuf_alloc(&con->mgr->txbuf_obj);
		// b = rdtsc();
		// printf("pbuf: %ld\n", b - a);
		size_t thislen = len - count;
		if(thislen > buflen) {
			thislen = buflen;
		}
		// a = rdtsc();
		memcpy(pb->data, buf + count, thislen);
		// b = rdtsc();
		// printf("data: %ld\n", b - a);
		// a = rdtsc();
		struct nstack_queue_entry nqe = nqe_build_send(con, pb->data, thislen);
		// b = rdtsc();
		// printf("bild: %ld\n", b - a); //SLOW

		// a = rdtsc();
		struct waiter_callback_info *info;
		info = waiter_callback_create(con->mgr, WAITER_INFO_AUTODESTROY);
		info->pbuf = pb;
		// b = rdtsc();
		// printf("info: %ld\n", b - a);
		// a = rdtsc();
		submit_tx(con->mgr, &nqe, __waiter_callback, info);
		// b = rdtsc();
		// printf("subm: %ld\n", b - a); //SLOW
		// netmgr_wait_all_tx_complete_until(con->mgr, __waiter_pred, &info);

		// pbuf_release(&con->mgr->txbuf_obj, pb);
		// waiter_callback_destroy(&info);
		count += thislen;
	}

	return count;
}

struct netcon *netcon_accept(struct netmgr *mgr)
{
	pthread_mutex_lock(&mgr->lock);
	while(1) {
		struct netevent *ev, *next;
		for(ev = mgr->eventlist_sentry.next; ev != &mgr->eventlist_sentry; ev = next) {
			next = ev->next;
			if(ev->event == NETEVENT_ACCEPT) {
				uint16_t cid = ev->info;
				__netevent_remove(ev);
				pthread_mutex_unlock(&mgr->lock);
				netevent_destroy(ev);
				struct netcon *newcon = netmgr_lookup_netcon(mgr, cid);
				return newcon;
			}
		}
		pthread_cond_wait(&mgr->event_cv, &mgr->lock);
	}
}

ssize_t netcon_recv(struct netcon *con, void *buf, size_t len, int flags)
{
	size_t count = 0;
	pthread_mutex_lock(&con->lock);
	while(count < len) {
		size_t rem = len - count;

		struct netevent *ev, *next;
		for(ev = con->eventlist_sentry.next; ev != &con->eventlist_sentry; ev = next) {
			next = ev->next;
			if(ev->event == NETEVENT_RX) {
				size_t thislen = rem;
				if(thislen > ev->data_len) {
					thislen = ev->data_len;
				}
				memcpy((char *)buf + count, ev->data_ptr, thislen);
				if(ev->data_len > thislen) {
					ev->data_ptr = (char *)ev->data_ptr + thislen;
					ev->data_len -= thislen;
				} else {
					__netevent_remove(ev);
					netevent_destroy(ev);
				}
				count += thislen;
			}
		}
		if(count == 0) {
			// pthread_mutex_unlock(&con->lock);
			// get_rx_commands(con->mgr, RX_ATLEASTONE);
			// pthread_mutex_lock(&con->lock);
			pthread_cond_wait(&con->event_cv, &con->lock);
		} else {
			break;
		}
	}
	pthread_mutex_unlock(&con->lock);
	return count;
}
