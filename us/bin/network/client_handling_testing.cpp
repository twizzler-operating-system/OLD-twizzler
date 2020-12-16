#include "client.h"

#define DEBUG 1

static id_allocator<uint16_t> tcp_client_id_alloc;

std::string netaddr_string(struct netaddr *na)
{
	std::string s = "netaddr(";
	s += std::to_string(na->type);
	s += ", ";
	for(int i = 0; i < 4; i++) {
		s += std::to_string((na->ipv4 >> (3 - i) * 8) & 0xff);
		if(i < 3)
			s += ".";
	}
	s += ":";
	s += std::to_string(na->port);
	s += ")";
	return s;
}

net_client::connection::connection(std::shared_ptr<net_client> client, uint16_t id, int tcp_id)
  : state(net_client::connection::NONE)
  , id(id)
  , client(client)
{
	tcp_client_id = tcp_id == -1 ? tcp_client_id_alloc.get() : tcp_id;
	thrd = std::thread(&net_client::connection::recv_thrd, this);
	fprintf(stderr, "created new connection for client %p\n", client.get());
}

ip_addr_t extract_ip_addr(struct netaddr *na)
{
	ip_addr_t ip;
	for(int i = 0; i < 4; i++) {
		ip.ip[i] = (na->ipv4 >> (3 - i) * 8) & 0xff;
	}
	return ip;
}

uint16_t extract_port(struct netaddr *na)
{
	return na->port;
}

int net_client::connection::bind(struct netaddr *na)
{
	std::lock_guard<std::mutex> _lg(lock);
	if(state != net_client::connection::NONE) {
		return -1; // TODO
	}
	uint16_t port = extract_port(na);
	ip_addr_t ip = extract_ip_addr(na);
	local.ip = ip;
	local.port = port;
	int ret = bind_to_tcp_port(tcp_client_id, ip, port);
	if(ret == 0) {
		state = net_client::connection::BOUND;
	}
	return ret;
}

int net_client::connection::connect(struct netaddr *na)
{
	std::lock_guard<std::mutex> _lg(lock);
	if(state != net_client::connection::NONE) {
		return -1; // TODO
	}
	remote.ip = extract_ip_addr(na);
	remote.port = extract_port(na);
	int ret = establish_tcp_conn_client(tcp_client_id, &local, remote);
	if(ret == 0) {
		state = net_client::connection::CONNECTED;
	}
	return ret;
}

int net_client::connection::accept(uint16_t *newid)
{
	std::lock_guard<std::mutex> _lg(lock);
	if(state != net_client::connection::BOUND) {
		return -1; // TODO
	}

	int ret = establish_tcp_conn_server(tcp_client_id, local, &remote);
	if(ret == 0) {
		*newid = client->create_connection(client, tcp_client_id);
		auto conn = client->get_connection(*newid);
		conn->local = local;
		conn->remote = remote;
		conn->state = net_client::connection::CONNECTED;
	}
	return ret;
}

ssize_t net_client::connection::send(std::shared_ptr<net_client> client,
  nstack_queue_entry *nqe,
  void *ptr,
  size_t len)
{
	std::lock_guard<std::mutex> _lg(lock);
	int ret = tcp_send(tcp_client_id, client, nqe, (char *)ptr, len);
	if(ret == 0)
		return len;
	return -ret;
}

void net_client::enqueue_cmd(struct nstack_queue_entry *nqe)
{
	/* TODO: handle error, non-blocking (?) */
	std::lock_guard<std::mutex> _lg(lock);
	if(queue_submit(&rxq_obj, (struct queue_entry *)nqe, QUEUE_NONBLOCK)) {
		fprintf(stderr, "warning - submission would have blocked\n");
	}
}

void submit_command(std::shared_ptr<net_client> client,
  struct nstack_queue_entry *nqe,
  void (*fn)(std::shared_ptr<net_client>, struct nstack_queue_entry *, void *),
  void *data)
{
	uint32_t id = client->outstanding_idalloc.get();
	auto cmd = std::make_shared<outstanding_command>(id, client, nqe, fn, data);
	nqe->qe.info = id;
#if DEBUG
	fprintf(
	  stderr, "submitting command to client %s: %d: %d\n", client->name.c_str(), id, nqe->cmd);
#endif
	client->push_outstanding(cmd, id);
	client->enqueue_cmd(nqe);
}

void net_client::connection::recv_thrd()
{
	kso_set_name(NULL, "net_client::connection::recv_thrd %d", id);
	for(;;) {
		lock.lock();
		if(state == net_client::connection::CONNECTED) {
			char buffer[5000];

			if(0) {
				lock.unlock();
				usleep(10000);
				continue;
			}
			int r = tcp_recv(tcp_client_id, buffer, 5000);

			if(r <= 0) {
				lock.unlock();
				usleep(10000);
				continue;
			}

			size_t off = client->testing_rxb_off;
			client->testing_rxb_off += r;
			if(client->testing_rxb_off >= OBJ_TOPDATA) {
				client->testing_rxb_off = OBJ_NULLPAGE_SIZE;
			}
			char *buf = (char *)twz_object_base(&client->rxbuf_obj);
			buf += off;
			memcpy(buf, buffer, r);
			struct nstack_queue_entry nqe;
			nqe.cmd = NSTACK_CMD_RECV;
			nqe.connid = id;
			nqe.data_ptr = twz_ptr_swizzle(&client->rxq_obj, buf, FE_READ | FE_WRITE);
			nqe.data_len = r;
			submit_command(client, &nqe, nullptr, nullptr);
			lock.unlock();
		} else if(state == net_client::connection::BOUND) {
			uint16_t cid;
			lock.unlock();
			if(this->accept(&cid) == 0) {
				struct nstack_queue_entry nqe;
				nqe.cmd = NSTACK_CMD_ACCEPT;
				nqe.connid = id;
				nqe.ret = cid;
				/* TODO: set addr */
				submit_command(client, &nqe, nullptr, nullptr);
				for(;;)
					usleep(1000000);
			} else {
				usleep(100);
			}
		} else {
			lock.unlock();
			usleep(1000);
		}
	}
}

static void __callback_complete(std::shared_ptr<net_client> client,
  struct nstack_queue_entry *nqe = nullptr,
  void *data = nullptr)
{
	(void)nqe;
	struct nstack_queue_entry *tc = (struct nstack_queue_entry *)data;
	client->complete(tc);
	delete tc;
}

bool handle_command(std::shared_ptr<net_client> client, struct nstack_queue_entry *nqe)
{
#if DEBUG
	fprintf(stderr,
	  "handling command from client %s: %d %d\n",
	  client->name.c_str(),
	  nqe->qe.info,
	  nqe->cmd);
#endif

	switch(nqe->cmd) {
		case NSTACK_CMD_CONNECT: {
			fprintf(stderr, "creating conn\n");
			uint16_t cid = client->create_connection(client);
			fprintf(stderr, "created connection %d\n", cid);
			if(cid == 0xffff) {
				nqe->ret = -ENOMEM; // TODO
				return true;
			}
			auto conn = client->get_connection(cid);
			if(!conn) {
				nqe->ret = -1; // TODO
				return true;
			}
			int ret = conn->connect(&nqe->addr);
			if(ret != 0) {
				client->remove_connection(cid);
			} else {
				nqe->connid = cid;
			}
			nqe->ret = ret;
			return true;
		} break;

		case NSTACK_CMD_BIND: {
			uint16_t cid = client->create_connection(client);
			if(cid == 0xffff) {
				nqe->ret = -ENOMEM;
				return true;
			}
			auto conn = client->get_connection(cid);
			if(!conn) {
				nqe->ret = -1; // TODO
				return true;
			}
			int ret = conn->bind(&nqe->addr);
			fprintf(stderr,
			  "client => bind %s :: %d, %d\n",
			  netaddr_string(&nqe->addr).c_str(),
			  ret,
			  cid);
			nqe->ret = ret;
			if(ret != 0) {
				client->remove_connection(cid);
			} else {
				nqe->connid = cid;
			}
			return true;
		} break;

		case NSTACK_CMD_SEND: {
			void *ptr = twz_object_lea(&client->txq_obj, nqe->data_ptr);
			auto conn = client->get_connection(nqe->connid);
			if(!conn) {
				nqe->ret = -ENOENT;
				return true;
			}

			size_t len = conn->send(client, nqe, ptr, nqe->data_len);
			nqe->ret = len;
			return false;

			/*
			struct nstack_queue_entry newnqe = *nqe;
			newnqe.cmd = NSTACK_CMD_RECV;
			size_t off = client->testing_rxb_off;
			client->testing_rxb_off += nqe->data_len;
			if(client->testing_rxb_off >= OBJ_TOPDATA) {
			    client->testing_rxb_off = OBJ_NULLPAGE_SIZE;
			}
			char *buf = (char *)twz_object_base(&client->rxbuf_obj);
			buf += off;
			memcpy(buf,
			  nqe->data_len); // should sanity check ptr
			newnqe.data_ptr = twz_ptr_swizzle(&client->rxq_obj, buf, FE_READ | FE_WRITE);
			submit_command(client, &newnqe, nullptr, nullptr);
			return true;*/
		} break;

#if 0
		case NSTACK_CMD_SEND: {
			struct nstack_queue_entry newnqe = *nqe;
			newnqe.cmd = NSTACK_CMD_RECV;
			size_t off = client->testing_rxb_off;
			client->testing_rxb_off += nqe->data_len;
			if(client->testing_rxb_off >= OBJ_TOPDATA) {
				client->testing_rxb_off = OBJ_NULLPAGE_SIZE;
			}
			char *buf = (char *)twz_object_base(&client->rxbuf_obj);
			buf += off;
			memcpy(buf,
			  twz_object_lea(&client->txq_obj, nqe->data_ptr),
			  nqe->data_len); // should sanity check ptr
			newnqe.data_ptr = twz_ptr_swizzle(&client->rxq_obj, buf, FE_READ | FE_WRITE);
			submit_command(client, &newnqe, nullptr, nullptr);
			return true;
		} break;
#endif
	}

	/* testing */
	// struct nstack_queue_entry newnqe;
	// newnqe.cmd = 789;
	// submit_command(client, &newnqe);

	return true;
}

void handle_completion(std::shared_ptr<net_client> client, struct nstack_queue_entry *nqe)
{
	std::shared_ptr<outstanding_command> cmd = client->pop_outstanding(nqe->qe.info);
	if(cmd == nullptr) {
		fprintf(stderr, "got completion for command that i don't know about! %d\n", nqe->qe.info);
		return;
	}

	cmd->complete();

#if DEBUG
	fprintf(
	  stderr, "got notified of completion from client %s: %d\n", client->name.c_str(), cmd->id);
#endif
	client->outstanding_idalloc.put(cmd->id);
}
