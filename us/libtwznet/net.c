#include <errno.h>
#include <nstack/net.h>
#include <nstack/nstack.h>
#include <stdio.h>
#include <stdlib.h>

struct netreq {
	struct nstack_queue_entry nqe;
	void (*callback)(struct netreq *req, struct nstack_queue_entry *nqe);
	void *data;
	struct netreq *next;
};

struct netevent *netevent_create(struct nstack_queue_entry *nqe,
  int event,
  int flags,
  void *ptr,
  size_t len)
{
	struct netevent *ev = malloc(sizeof(*ev));
	ev->event = event;
	ev->flags = flags;
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
	req->nqe.qe.info = id;
	pthread_mutex_lock(&mgr->lock);

	req->next = mgr->outstanding;
	mgr->outstanding = req;

	pthread_mutex_unlock(&mgr->lock);
}

static void handle_completion_from_tx(struct netmgr *mgr, struct nstack_queue_entry *nqe)
{
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
	pthread_mutex_unlock(&mgr->lock);
	fprintf(stderr, "could not find outstanding request for ID %d\n", nqe->qe.info);
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
			fprintf(stderr, "got recv event!\n");
			struct netcon *con = netmgr_lookup_netcon(mgr, nqe->connid);
			struct netevent *ev = netevent_create(
			  nqe, NETEVENT_RX, 0, twz_object_lea(&mgr->rxq_obj, nqe->data_ptr), nqe->data_len);
			pthread_mutex_lock(&con->lock);
			__netevent_append(&con->eventlist_sentry, ev);
			pthread_cond_signal(&con->event_cv);
			pthread_mutex_unlock(&con->lock);
			complete = false;
		} break;
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
		if(queue_receive(&mgr->rxq_obj, (struct queue_entry *)&nqe, non_block ? QUEUE_NONBLOCK : 0)
		   == 0) {
		} else if(!non_block) {
			//	pthread_mutex_unlock(&mgr->lock);
			return;
		}
		// pthread_mutex_unlock(&mgr->lock);
		handle_cmd(mgr, &nqe);
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
	struct netreq *req = malloc(sizeof(*req));
	req->callback = callback;
	req->data = data;
	req->nqe = *nqe;
	fprintf(stderr, "!! adding out\n");
	add_outstanding_tx(mgr, req);
	fprintf(stderr, "!! doing submit\n");
	queue_submit(&mgr->txq_obj, (struct queue_entry *)&req->nqe, 0);
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
	long ret;
	bool done;
};

static inline void waiter_callback_init(struct waiter_callback_info *info)
{
	pthread_mutex_init(&info->lock, NULL);
	info->ret = 0;
	info->done = false;
}

static inline void waiter_callback_destroy(struct waiter_callback_info *info)
{
	pthread_mutex_destroy(&info->lock);
}

static void __waiter_callback(struct netreq *req, struct nstack_queue_entry *nqe)
{
	struct waiter_callback_info *info = req->data;
	pthread_mutex_lock(&info->lock);
	info->done = true;
	info->ret = nqe->ret;
	pthread_mutex_unlock(&info->lock);
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

struct netcon *netmgr_connect(struct netmgr *mgr,
  struct netaddr *addr,
  int flags,
  struct timespec *timeout)
{
	struct waiter_callback_info info;
	struct nstack_queue_entry nqe = nqe_build_connect(addr, flags);
	waiter_callback_init(&info);
	submit_tx(mgr, &nqe, __waiter_callback, &info);
	netmgr_wait_all_tx_complete_until(mgr, __waiter_pred, &info);

	if(info.ret < 0) {
		errno = -info.ret;
		return NULL;
	}

	struct netcon *con = malloc(sizeof(*con));
	con->id = info.ret;
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
	while(count < len) {
		fprintf(stderr, "alloc buf\n");
		struct pbuf *pb = pbuf_alloc(&con->mgr->txbuf_obj);
		fprintf(stderr, "send: pbuf: %p\n", pb);
		size_t thislen = len - count;
		if(thislen > buflen) {
			thislen = buflen;
		}
		fprintf(stderr, "send: thislen = %ld\n", thislen);
		memcpy(pb->data, buf + count, thislen);
		struct nstack_queue_entry nqe = nqe_build_send(con, pb->data, thislen);

		struct waiter_callback_info info;
		waiter_callback_init(&info);
		info.done = false;
		submit_tx(con->mgr, &nqe, __waiter_callback, &info);

		netmgr_wait_all_tx_complete_until(con->mgr, __waiter_pred, &info);

		fprintf(stderr, "ret = %ld\n", info.ret);

		pbuf_release(&con->mgr->txbuf_obj, pb);
		waiter_callback_destroy(&info);
		count += thislen;
	}

	return count;
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
				}
				count += thislen;
			}
		}
		if(count == 0) {
			pthread_cond_wait(&con->event_cv, &con->lock);
		} else {
			break;
		}
	}
	pthread_mutex_unlock(&con->lock);
	return count;
}
