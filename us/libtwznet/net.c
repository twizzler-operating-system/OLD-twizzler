#include <errno.h>
#include <nstack/net.h>
#include <nstack/nstack.h>
#include <stdio.h>
#include <stdlib.h>

struct netreq {
	struct nstack_queue_entry nqe;
	void (*callback)(struct netreq *req);
	void *data;
	struct netreq *next;
};

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
				req->callback(req);
			}
			release_id(mgr, req->nqe.qe.info);
			free(req);
			return;
		}
	}
	pthread_mutex_unlock(&mgr->lock);
	fprintf(stderr, "could not find outstanding request for ID %d\n", nqe->qe.info);
}

static void handle_cmd(struct netmgr *mgr, struct nstack_queue_entry *nqe)
{
}

#define RX_NONBLOCK 1
#define RX_LIMIT 2

#define THRESH 64

static void get_rx_commands(struct netmgr *mgr, int flags)
{
	struct nstack_queue_entry nqe;
	size_t i = 0;
	bool non_block = !!(flags & RX_NONBLOCK);
	while(1) {
		pthread_mutex_lock(&mgr->lock);
		if(queue_receive(&mgr->rxq_obj, (struct queue_entry *)&nqe, non_block ? QUEUE_NONBLOCK : 0)
		   == 0) {
		} else if(!non_block) {
			pthread_mutex_unlock(&mgr->lock);
			return;
		}
		pthread_mutex_unlock(&mgr->lock);
		handle_cmd(mgr, &nqe);
		if((i++ > THRESH) && (flags & RX_LIMIT)) {
			return;
		}
	}
}

static void submit_tx(struct netmgr *mgr,
  struct nstack_queue_entry *nqe,
  void (*callback)(struct netreq *),
  void *data)
{
	struct netreq *req = malloc(sizeof(*req));
	req->callback = callback;
	req->data = data;
	req->nqe = *nqe;
	add_outstanding_tx(mgr, req);
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
	get_rx_commands(mgr, RX_NONBLOCK | RX_LIMIT);
}

static void __echo_callback(struct netreq *req)
{
	fprintf(stderr, ":: echo callback %p\n", req->data);
}

void netmgr_echo(struct netmgr *mgr)
{
	struct nstack_queue_entry nqe = {};
	submit_tx(mgr, &nqe, __echo_callback, (void *)1234);

	complete_tx(mgr);
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

struct send_callback_info {
	struct pbuf *buf;
	pthread_mutex_t lock;
	pthread_cond_t cv;
};

static void __send_callback(struct netreq *req)
{
	struct send_callback_info *info = req->data;
	pthread_mutex_lock(&info->lock);
	pthread_cond_signal(&info->cv);
	pthread_mutex_unlock(&info->lock);
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
		struct pbuf *buf = pbuf_alloc(&con->mgr->txbuf_obj);
		size_t thislen = len - count;
		if(thislen > buflen) {
			thislen = buflen;
		}
		memcpy(buf->data, buf + count, thislen);
		struct nstack_queue_entry nqe = nqe_build_send(con, buf->data, thislen);

		struct send_callback_info info;
		pthread_mutex_init(&info.lock, NULL);
		pthread_mutex_lock(&info.lock);
		pthread_cond_init(&info.cv, NULL);
		submit_tx(con->mgr, &nqe, __send_callback, &info);
		pthread_cond_wait(&info.cv, &info.lock);
		pthread_mutex_unlock(&info.lock);
		pthread_mutex_destroy(&info.lock);
		pthread_cond_destroy(&info.cv);
		count += thislen;
	}

	return count;
}
