#include "tcp_conn.h"

#include "encapsulate.h"
#include "interface.h"
#include "ipv4.h"
#include "twz.h"

std::map<half_conn_t, std::vector<outstanding_conn_t>, cmp1> outstanding_conns;
std::mutex outstanding_conns_mutex;

bool compare_half_conn(half_conn_t half_conn_1, half_conn_t half_conn_2)
{
	ip_addr_t default_ip = string_to_ip_addr(DEFAULT_IP);

	if(!compare_ip_addr(half_conn_1.ip, default_ip, default_ip)
	   && !compare_ip_addr(half_conn_2.ip, default_ip, default_ip)) {
		for(int i = 0; i < IP_ADDR_SIZE; ++i) {
			if(half_conn_1.ip.ip[i] != half_conn_2.ip.ip[i]) {
				return false;
			}
		}
	}

	if(half_conn_1.port != half_conn_2.port) {
		return false;
	}

	return true;
}

void outstanding_conns_enqueue(half_conn_t half_conn, outstanding_conn_t outstanding_conn)
{
	std::map<half_conn_t, std::vector<outstanding_conn_t>>::iterator it;

	outstanding_conns_mutex.lock();

	bool found = false;
	for(it = outstanding_conns.begin(); it != outstanding_conns.end(); ++it) {
		if(compare_half_conn(half_conn, it->first)) {
			found = true;
			break;
		}
	}
	if(!found) {
		std::vector<outstanding_conn_t> outstanding_conn_list;
		outstanding_conn_list.push_back(outstanding_conn);
		outstanding_conns[half_conn] = outstanding_conn_list;
	} else {
		it->second.push_back(outstanding_conn);
	}

	outstanding_conns_mutex.unlock();
}

outstanding_conn_t outstanding_conns_dequeue(half_conn_t half_conn)
{
	std::map<half_conn_t, std::vector<outstanding_conn_t>>::iterator it;

	outstanding_conns_mutex.lock();

	bool found = false;
	for(it = outstanding_conns.begin(); it != outstanding_conns.end(); ++it) {
		if(compare_half_conn(half_conn, it->first)) {
			found = true;
			break;
		}
	}

	outstanding_conn_t conn;
	if(!found) {
		conn.is_valid = false;
	} else {
		conn = it->second.front();
		it->second.erase(it->second.begin());
	}

	outstanding_conns_mutex.unlock();

	return conn;
}

void outstanding_conns_delete(half_conn_t half_conn)
{
	std::map<half_conn_t, std::vector<outstanding_conn_t>>::iterator it;

	outstanding_conns_mutex.lock();

	bool found = false;
	for(it = outstanding_conns.begin(); it != outstanding_conns.end(); ++it) {
		if(compare_half_conn(half_conn, it->first)) {
			found = true;
			break;
		}
	}

	if(found) {
		outstanding_conns.erase(it);
	}

	outstanding_conns_mutex.unlock();
}

void outstanding_conns_view()
{
	std::map<half_conn_t, std::vector<outstanding_conn_t>>::iterator it;

	outstanding_conns_mutex.lock();

	fprintf(stderr, "OUTSTANDING CONN TABLE:\n");
	fprintf(stderr, "------------------------------------------\n");
	for(it = outstanding_conns.begin(); it != outstanding_conns.end(); ++it) {
		fprintf(stderr,
		  "[%u.%u.%u.%u, %u] -> [ ",
		  (it->first.ip.ip[0] & 0x000000FF),
		  (it->first.ip.ip[1] & 0x000000FF),
		  (it->first.ip.ip[2] & 0x000000FF),
		  (it->first.ip.ip[3] & 0x000000FF),
		  (it->first.port & 0x0000FFFF));
		std::vector<outstanding_conn_t>::iterator it1;
		for(it1 = it->second.begin(); it1 != it->second.end(); ++it1) {
			fprintf(stderr,
			  "[%d, %u.%u.%u.%u, %u, %u] ",
			  it1->is_valid,
			  (it1->remote.ip.ip[0] & 0x000000FF),
			  (it1->remote.ip.ip[1] & 0x000000FF),
			  (it1->remote.ip.ip[2] & 0x000000FF),
			  (it1->remote.ip.ip[3] & 0x000000FF),
			  (it1->remote.port & 0x0000FFFF),
			  it1->seq_num);
		}
		fprintf(stderr, "]\n");
	}
	fprintf(stderr, "------------------------------------------\n");

	outstanding_conns_mutex.unlock();
}

struct cmp2 {
	bool operator()(const tcp_conn_t a, const tcp_conn_t b) const
	{
		return a.local.port < b.local.port;
	}
};

std::map<tcp_conn_t, tcp_conn_state_t *, cmp2> conn_table;
std::map<tcp_conn_t, tcp_conn_state_t *, cmp2>::iterator head;
std::vector<tcp_conn_state_t *> inactive_conn_list;
bool conn_table_bootstrap_done = false;
std::mutex conn_table_mutex;

bool compare_tcp_conn(tcp_conn_t conn_1, tcp_conn_t conn_2)
{
	ip_addr_t default_ip = string_to_ip_addr(DEFAULT_IP);

	if(!compare_ip_addr(conn_1.local.ip, default_ip, default_ip)
	   && !compare_ip_addr(conn_2.local.ip, default_ip, default_ip)) {
		for(int i = 0; i < IP_ADDR_SIZE; ++i) {
			if(conn_1.local.ip.ip[i] != conn_2.local.ip.ip[i]) {
				return false;
			}
		}
	}

	if(!compare_ip_addr(conn_1.remote.ip, default_ip, default_ip)
	   && !compare_ip_addr(conn_2.remote.ip, default_ip, default_ip)) {
		for(int i = 0; i < IP_ADDR_SIZE; ++i) {
			if(conn_1.remote.ip.ip[i] != conn_2.remote.ip.ip[i]) {
				return false;
			}
		}
	}

	if(conn_1.local.port != conn_2.local.port) {
		return false;
	}

	if(conn_1.remote.port != conn_2.remote.port) {
		return false;
	}

	return true;
}

void conn_table_put(tcp_conn_t conn, tcp_conn_state_t *conn_state)
{
	std::map<tcp_conn_t, tcp_conn_state_t *>::iterator it;

	conn_table_mutex.lock();

	bool found = false;
	for(it = conn_table.begin(); it != conn_table.end(); ++it) {
		if(compare_tcp_conn(conn, it->first)) {
			found = true;
			break;
		}
	}

	if(!found) {
		conn_table[conn] = conn_state;
	} else {
		it->second = conn_state;
	}

	if(head == conn_table.begin()) {
		++head;
	}

	conn_table_mutex.unlock();
}

tcp_conn_state_t *conn_table_get(tcp_conn_t conn)
{
	std::map<tcp_conn_t, tcp_conn_state_t *>::iterator it;

	conn_table_mutex.lock();

	bool found = false;
	for(it = conn_table.begin(); it != conn_table.end(); ++it) {
		if(compare_tcp_conn(conn, it->first)) {
			found = true;
			break;
		}
	}

	conn_table_mutex.unlock();

	if(!found) {
		return NULL;
	} else {
		return it->second;
	}
}

void conn_table_delete(tcp_conn_t conn)
{
	std::map<tcp_conn_t, tcp_conn_state_t *>::iterator it;

	conn_table_mutex.lock();

	bool found = false;
	for(it = conn_table.begin(); it != conn_table.end(); ++it) {
		if(compare_tcp_conn(conn, it->first)) {
			found = true;
			break;
		}
	}

	if(found) {
		tcp_conn_state_t *conn_state = it->second;

		// pthread_spin_destroy(&conn_state->mtx);
		// free_char_ring_buffer(conn_state->tx_buffer);
		// free_char_ring_buffer(conn_state->rx_buffer);
		// free(conn_state);
		inactive_conn_list.push_back(conn_state); // TODO free conn_state

		conn_table.erase(it);
	}

	if(head == it) {
		if(++head == conn_table.end()) {
			head = conn_table.begin();
			if(++head == conn_table.end()) {
				head = conn_table.begin();
			}
		}
	}

	conn_table_mutex.unlock();
}

void conn_table_view()
{
	std::map<tcp_conn_t, tcp_conn_state_t *>::iterator it;

	conn_table_mutex.lock();

	fprintf(stderr, "TCP CONN TABLE:\n");
	fprintf(stderr, "------------------------------------------\n");
	for(it = conn_table.begin(); it != conn_table.end(); ++it) {
		if(it == conn_table.begin())
			continue;
		pthread_spin_lock(&it->second->mtx);
		fprintf(stderr,
		  "[%u.%u.%u.%u, %u.%u.%u.%u, %u, %u] -> "
		  "[client_id: %u, seq_num: %u, ack_num: %u]\n",
		  (it->first.local.ip.ip[0] & 0x000000FF),
		  (it->first.local.ip.ip[1] & 0x000000FF),
		  (it->first.local.ip.ip[2] & 0x000000FF),
		  (it->first.local.ip.ip[3] & 0x000000FF),
		  (it->first.remote.ip.ip[0] & 0x000000FF),
		  (it->first.remote.ip.ip[1] & 0x000000FF),
		  (it->first.remote.ip.ip[2] & 0x000000FF),
		  (it->first.remote.ip.ip[3] & 0x000000FF),
		  (it->first.local.port & 0x0000FFFF),
		  (it->first.remote.port & 0x0000FFFF),
		  it->second->client_id,
		  it->second->seq_num,
		  it->second->ack_num);
		pthread_spin_unlock(&it->second->mtx);
	}
	fprintf(stderr, "------------------------------------------\n");

	conn_table_mutex.unlock();
}

tcp_conn_state_t *get_next_conn_state(tcp_conn_t *conn)
{
	tcp_conn_state_t *conn_state = NULL;

	conn_table_mutex.lock();

	if(conn_table_bootstrap_done && head != conn_table.begin()) {
		memcpy(conn->local.ip.ip, head->first.local.ip.ip, IP_ADDR_SIZE);
		memcpy(conn->remote.ip.ip, head->first.remote.ip.ip, IP_ADDR_SIZE);
		conn->local.port = head->first.local.port;
		conn->remote.port = head->first.remote.port;

		++head;
		if(head == conn_table.end()) {
			head = conn_table.begin();
			++head;
		}

		conn_state = head->second;
	}

	conn_table_mutex.unlock();

	return conn_state;
}

void bootstrap_conn_table()
{
	tcp_conn_t conn;
	memcpy(conn.local.ip.ip, string_to_ip_addr(DEFAULT_IP).ip, IP_ADDR_SIZE);
	memcpy(conn.remote.ip.ip, string_to_ip_addr(DEFAULT_IP).ip, IP_ADDR_SIZE);
	conn.local.port = 0;
	conn.remote.port = 0;

	conn_table_put(conn, NULL);

	conn_table_mutex.lock();
	head = conn_table.begin();
	conn_table_mutex.unlock();

	conn_table_bootstrap_done = true;
}

std::map<uint16_t, tcp_conn_t> tcp_client_table;
std::mutex tcp_client_table_mutex;

void tcp_client_table_put(uint16_t client_id, tcp_conn_t conn)
{
	std::map<uint16_t, tcp_conn_t>::iterator it;

	tcp_client_table_mutex.lock();

	bool found = false;
	for(it = tcp_client_table.begin(); it != tcp_client_table.end(); ++it) {
		if(it->first == client_id) {
			found = true;
			break;
		}
	}

	if(!found) {
		tcp_client_table[client_id] = conn;
	} else {
		it->second = conn;
	}

	tcp_client_table_mutex.unlock();
}

tcp_conn_t tcp_client_table_get(uint16_t client_id)
{
	std::map<uint16_t, tcp_conn_t>::iterator it;

	tcp_client_table_mutex.lock();

	bool found = false;
	for(it = tcp_client_table.begin(); it != tcp_client_table.end(); ++it) {
		if(it->first == client_id) {
			found = true;
			break;
		}
	}

	tcp_client_table_mutex.unlock();

	if(!found) {
		tcp_conn_t conn;
		conn.local.port = 0;
		conn.remote.port = 0;
		return conn;
	} else {
		return it->second;
	}
}

void tcp_client_table_delete(uint16_t client_id)
{
	std::map<uint16_t, tcp_conn_t>::iterator it;

	tcp_client_table_mutex.lock();

	bool found = false;
	for(it = tcp_client_table.begin(); it != tcp_client_table.end(); ++it) {
		if(it->first == client_id) {
			found = true;
			break;
		}
	}

	if(found) {
		tcp_client_table.erase(it);
	}

	tcp_client_table_mutex.unlock();
}

void tcp_client_table_view()
{
	std::map<uint16_t, tcp_conn_t>::iterator it;

	tcp_client_table_mutex.lock();

	fprintf(stderr, "TCP CLIENT TABLE:\n");
	fprintf(stderr, "------------------------------------------\n");
	for(it = tcp_client_table.begin(); it != tcp_client_table.end(); ++it) {
		fprintf(stderr,
		  "%u -> [%u.%u.%u.%u, %u.%u.%u.%u, %u, %u]\n",
		  it->first,
		  (it->second.local.ip.ip[0] & 0x000000FF),
		  (it->second.local.ip.ip[1] & 0x000000FF),
		  (it->second.local.ip.ip[2] & 0x000000FF),
		  (it->second.local.ip.ip[3] & 0x000000FF),
		  (it->second.remote.ip.ip[0] & 0x000000FF),
		  (it->second.remote.ip.ip[1] & 0x000000FF),
		  (it->second.remote.ip.ip[2] & 0x000000FF),
		  (it->second.remote.ip.ip[3] & 0x000000FF),
		  (it->second.local.port & 0x0000FFFF),
		  (it->second.remote.port & 0x0000FFFF));
	}
	fprintf(stderr, "------------------------------------------\n");

	tcp_client_table_mutex.unlock();
}

uint8_t tcp_ports[65536] = { 0 };
std::mutex tcp_port_mtx;

int bind_to_tcp_port(uint16_t client_id, ip_addr_t ip, uint16_t port)
{
	if(bind_to_ip(ip) != 0) {
		return ETCPBINDFAILED;
	}

	if(port == 0) {
		fprintf(stderr, "Error bind_to_port: port number cannot be 0\n");
		return ETCPBINDFAILED;
	}

	tcp_port_mtx.lock();

	if(tcp_ports[port] == 0) {
		tcp_ports[port] = 1;
		tcp_port_mtx.unlock();
		return 0;
	} else {
		fprintf(stderr, "Error bind_to_tcp_port: port %d already in use\n", port);
		tcp_port_mtx.unlock();
		return ETCPBINDFAILED;
	}
}

uint16_t bind_to_random_tcp_port()
{
	tcp_port_mtx.lock();

	/* TODO: grab from ephemeral range */
	for(uint32_t i = 1; i < 65536; ++i) {
		if(tcp_ports[i] == 0) {
			tcp_ports[i] = 1;
			tcp_port_mtx.unlock();
			return i;
		}
	}

	fprintf(stderr, "Error bind_to_random_port: no free ports available\n");
	exit(1);
}

void free_tcp_port(uint16_t port)
{
	tcp_port_mtx.lock();

	if(tcp_ports[port] == 1) {
		tcp_ports[port] = 0;
		tcp_port_mtx.unlock();
		return;
	}
}

tcp_endpoint::tcp_endpoint(std::shared_ptr<net_client> client,
  uint16_t cid,
  tcp_conn_t conn,
  tcp_state_t state)
  : client(client)
  , client_cid(cid)
  , conn(conn)
{
	pthread_spin_init(&conn_state.mtx, 0);
	conn_state.seq_num = 0;
	conn_state.ack_num = 0;
	conn_state.tx_buf_mgr = new databuf();
	conn_state.rx_buf_mgr = new databuf();
	conn_state.lock = new std::mutex();
	conn_state.rx_buffer = create_char_ring_buffer(CHAR_RING_BUFFER_SIZE);
	conn_state.tx_head = 1;
	conn_state.curr_state = state;
}

tcp_endpoint::tcp_endpoint(std::shared_ptr<net_client> client, uint16_t cid)
  : client(client)
  , client_cid(cid)
{
	pthread_spin_init(&conn_state.mtx, 0);
	conn_state.seq_num = 0;
	conn_state.ack_num = 0;
	conn_state.tx_buf_mgr = new databuf();
	conn_state.rx_buf_mgr = new databuf();
	conn_state.lock = new std::mutex();
	conn_state.rx_buffer = create_char_ring_buffer(CHAR_RING_BUFFER_SIZE);
	conn_state.tx_head = 1;
	conn_state.curr_state = SYN_SENT;
}

std::map<tcp_conn_t, std::shared_ptr<tcp_endpoint>, cmp2> connections;
std::mutex connections_lock;

int tcp_endpoint_connect(std::shared_ptr<tcp_endpoint> endp, half_conn_t remote)
{
	char interface_name[MAX_INTERFACE_NAME_SIZE];
	ip_table_get(remote.ip, interface_name);
	if(interface_name == NULL) {
		fprintf(stderr,
		  "Error establish_tcp_conn_client: "
		  "ip_table_get returned no valid interface\n");
		exit(1);
	}
	interface_t *interface = get_interface_by_name(interface_name);

	memcpy(endp->conn.local.ip.ip, interface->ip.ip, IP_ADDR_SIZE);
	memcpy(endp->conn.remote.ip.ip, remote.ip.ip, IP_ADDR_SIZE);
	endp->conn.local.port = bind_to_random_tcp_port();
	endp->conn.remote.port = remote.port;

	endp->conn_state.curr_state = SYN_SENT;

	// conn_table_put(conn, conn_state);

	// object_id_t object_id;
	// endp->conn_state.curr_state = CONN_ESTABLISHED;

	{
		std::lock_guard<std::mutex> _lg(connections_lock);
		if(connections.find(endp->conn) != connections.end()) {
			/* TODO */
			return -1;
		}
		connections.insert(std::make_pair(endp->conn, endp));
	}
	/* send SYN packet */
	int ret = encap_tcp_packet_2(endp->conn.local.ip,
	  endp->conn.remote.ip,
	  endp->conn.local.port,
	  endp->conn.remote.port,
	  SYN_PKT,
	  endp->conn_state.seq_num,
	  endp->conn_state.ack_num,
	  NULL,
	  0);
	/* TODO: err */

	/*
	tcp_client_table_put(client_id, conn);

	if(local != NULL) {
	    memcpy(local->ip.ip, conn.local.ip.ip, IP_ADDR_SIZE);
	    local->port = conn.local.port;
	}

	tcp_client_table_view();
	conn_table_view();
*/
	return 0;
}

#if 0
int establish_tcp_conn_client(uint16_t client_id, half_conn_t *local, half_conn_t remote)
{
	char interface_name[MAX_INTERFACE_NAME_SIZE];
	ip_table_get(remote.ip, interface_name);
	if(interface_name == NULL) {
		fprintf(stderr,
		  "Error establish_tcp_conn_client: "
		  "ip_table_get returned no valid interface\n");
		exit(1);
	}
	interface_t *interface = get_interface_by_name(interface_name);

	tcp_conn_t conn;
	memcpy(conn.local.ip.ip, interface->ip.ip, IP_ADDR_SIZE);
	memcpy(conn.remote.ip.ip, remote.ip.ip, IP_ADDR_SIZE);
	conn.local.port = bind_to_random_tcp_port();
	conn.remote.port = remote.port;

	tcp_conn_state_t *conn_state = (tcp_conn_state_t *)malloc(sizeof(tcp_conn_state_t));

	pthread_spin_init(&conn_state->mtx, 0);
	conn_state->client_id = client_id;
	conn_state->curr_state = SYN_SENT;
	conn_state->seq_num = 0;
	conn_state->ack_num = 0;
	conn_state->tx_buf_mgr = new databuf();
	conn_state->rx_buf_mgr = new databuf();
	conn_state->lock = new std::mutex();
	// conn_state->tx_buffer = create_char_ring_buffer(CHAR_RING_BUFFER_SIZE);
	conn_state->rx_buffer = create_char_ring_buffer(CHAR_RING_BUFFER_SIZE);
	conn_state->tx_head = 1;
	uint32_t seq_num = conn_state->seq_num;
	uint32_t ack_num = conn_state->ack_num;

	conn_table_put(conn, conn_state);

	object_id_t object_id;
	/* send SYN packet */
	int ret = encap_tcp_packet(object_id,
	  NOOP,
	  conn.local.ip,
	  conn.remote.ip,
	  conn.local.port,
	  conn.remote.port,
	  SYN_PKT,
	  seq_num,
	  ack_num,
	  NULL,
	  0);
	if(ret != 0) {
		cleanup_tcp_conn(conn);
		return ETCPCONNFAILED;
	}

	clock_t start = clock();
	while(conn_state->curr_state != SYN_ACK_RECVD) {
		usleep(10);

		clock_t time_elapsed_msec = ((clock() - start) * 1000) / CLOCKS_PER_SEC;
		if(time_elapsed_msec >= TCP_TIMEOUT) {
			cleanup_tcp_conn(conn);
			return ETCPCONNFAILED;
		}
	}

	/* send ACK packet */
	ret = encap_tcp_packet(object_id,
	  NOOP,
	  conn.local.ip,
	  conn.remote.ip,
	  conn.local.port,
	  conn.remote.port,
	  ACK_PKT,
	  conn_state->seq_num,
	  conn_state->ack_num,
	  NULL,
	  0);
	if(ret != 0) {
		cleanup_tcp_conn(conn);
		return ETCPCONNFAILED;
	}

	pthread_spin_lock(&conn_state->mtx);
	conn_state->curr_state = CONN_ESTABLISHED;
	// assert(conn_state->seq_num == 1 && conn_state->tx_buffer->head == 1
	//     && conn_state->tx_buffer->tail == 1 && conn_state->rx_buffer->head == 1
	//   && conn_state->rx_buffer->tail == 1);
	pthread_spin_unlock(&conn_state->mtx);

	tcp_client_table_put(client_id, conn);

	if(local != NULL) {
		memcpy(local->ip.ip, conn.local.ip.ip, IP_ADDR_SIZE);
		local->port = conn.local.port;
	}

	tcp_client_table_view();
	conn_table_view();

	return 0;
}
#endif

std::map<half_conn_t, std::shared_ptr<tcp_rendezvous>, cmp1> listeners; // of roshar
std::mutex listeners_lock;

int tcp_rendezvous_start_listen(std::shared_ptr<tcp_rendezvous> tr)
{
	std::lock_guard<std::mutex> _lg(listeners_lock);
	auto [_, res] = listeners.emplace(std::make_pair(tr->point, tr));
	return res ? 0 : -EEXIST;
}

void tcp_rendezvous_start_accept(std::shared_ptr<tcp_rendezvous> tr)
{
	std::lock_guard<std::mutex> _lg(listeners_lock);
	tr->ready = true;
	while(1) {
		outstanding_conn_t remote_conn = outstanding_conns_dequeue(tr->point);
		if(!remote_conn.is_valid)
			break;
		tr->accept(remote_conn);
	}
}

void tcp_rendezvous::accept(outstanding_conn_t &remote_conn)
{
	std::lock_guard<std::mutex> _lg(connections_lock);

	tcp_conn_t conn;
	memcpy(conn.local.ip.ip, point.ip.ip, IP_ADDR_SIZE);
	memcpy(conn.remote.ip.ip, remote_conn.remote.ip.ip, IP_ADDR_SIZE);
	conn.local.port = point.port;
	conn.remote.port = remote_conn.remote.port;

	if(connections.find(conn) != connections.end()) {
		/* TODO */
		return;
	}

	auto endp =
	  std::make_shared<tcp_endpoint>(client, client->conn_idalloc.get(), conn, SYN_ACK_SENT);
	connections.insert(std::make_pair(conn, endp));

	endp->conn_state.ack_num = remote_conn.seq_num + 1;
	/* TODO: set timeout? */
	client->insert_connection(endp);

	int ret = encap_tcp_packet_2(point.ip,
	  remote_conn.remote.ip,
	  point.port,
	  remote_conn.remote.port,
	  SYN_ACK_PKT,
	  0,
	  remote_conn.seq_num + 1,
	  NULL,
	  0);
	/* TODO: error */
}

#if 0
int establish_tcp_conn_server(uint16_t client_id, half_conn_t local, half_conn_t *remote)
{
	/* extract an outstanding TCP conn req */
	outstanding_conn_t remote_conn = outstanding_conns_dequeue(local);
	if(!remote_conn.is_valid) {
		return ETCPCONNFAILED;
	}

	tcp_conn_t conn;
	memcpy(conn.local.ip.ip, local.ip.ip, IP_ADDR_SIZE);
	memcpy(conn.remote.ip.ip, remote_conn.remote.ip.ip, IP_ADDR_SIZE);
	conn.local.port = local.port;
	conn.remote.port = remote_conn.remote.port;

	if(conn_table_get(conn) != NULL) { /* connection already exists! */
		return ETCPCONNFAILED;
	}

	tcp_conn_state_t *conn_state = (tcp_conn_state_t *)malloc(sizeof(tcp_conn_state_t));

	pthread_spin_init(&conn_state->mtx, 0);
	conn_state->curr_state = SYN_ACK_SENT;
	conn_state->client_id = client_id;
	conn_state->seq_num = 0;
	conn_state->ack_num = remote_conn.seq_num + 1;
	conn_state->tx_head = 1;
	conn_state->tx_buf_mgr = new databuf();
	conn_state->rx_buf_mgr = new databuf();
	conn_state->lock = new std::mutex();
	// conn_state->tx_buffer = create_char_ring_buffer(CHAR_RING_BUFFER_SIZE);
	conn_state->rx_buffer = create_char_ring_buffer(CHAR_RING_BUFFER_SIZE);
	uint32_t seq_num = conn_state->seq_num;
	uint32_t ack_num = conn_state->ack_num;

	conn_table_put(conn, conn_state);

	object_id_t object_id;
	/* send SYN-ACK packet */
	int ret = encap_tcp_packet(object_id,
	  NOOP,
	  conn.local.ip,
	  conn.remote.ip,
	  conn.local.port,
	  conn.remote.port,
	  SYN_ACK_PKT,
	  seq_num,
	  ack_num,
	  NULL,
	  0);
	if(ret != 0) {
		cleanup_tcp_conn(conn);
		/* re-enqueue to try again */
		outstanding_conns_enqueue(local, remote_conn);
		return ETCPCONNFAILED;
	}

	clock_t start = clock();
	bool recvd_ack = true;
	while(conn_state->curr_state != ACK_RECVD) {
		usleep(10);

		clock_t time_elapsed_msec = ((clock() - start) * 1000) / CLOCKS_PER_SEC;
		if(time_elapsed_msec >= TCP_TIMEOUT) {
			cleanup_tcp_conn(conn);
			recvd_ack = false;
			break;
		}
	}
	if(!recvd_ack) { /* issue with the client; don't re-enqueue;
		              * instead try some other outstanding conn */
		return ETCPCONNFAILED;
	}

	pthread_spin_lock(&conn_state->mtx);
	conn_state->curr_state = CONN_ESTABLISHED;
	/*assert(conn_state->seq_num == 1 && conn_state->tx_buffer->head == 1
	       && conn_state->tx_buffer->tail == 1 && conn_state->rx_buffer->head == 1
	       && conn_state->rx_buffer->tail == 1);*/
	pthread_spin_unlock(&conn_state->mtx);

	tcp_client_table_put(client_id, conn);

	if(remote != NULL) {
		memcpy(remote->ip.ip, conn.remote.ip.ip, IP_ADDR_SIZE);
		remote->port = conn.remote.port;
	}

	tcp_client_table_view();
	conn_table_view();

	return 0;
}
#endif

static void __try_send(tcp_conn_t &conn, tcp_conn_state_t *conn_state, size_t amount)
{
	int count = 0;
	while(true) {
		uint32_t seq_num = conn_state->seq_num;
		uint32_t ack_num = conn_state->ack_num;
		std::lock_guard<std::mutex> _lg(*conn_state->lock);
		auto data = conn_state->tx_buf_mgr->get_next(MSS);
		if(data.ptr) {
			// fprintf(stderr, "SEND %d bytes\n", data.len);
			int ret = encap_tcp_packet_2(conn.local.ip,
			  conn.remote.ip,
			  conn.local.port,
			  conn.remote.port,
			  DATA_PKT,
			  seq_num,
			  ack_num,
			  (char *)data.ptr,
			  data.len);
			if(ret) {
				fprintf(stderr, " -- Packet Dropped!!\n");
			}
			if(amount) {
				if(amount < data.len)
					amount = 0;
				else
					amount -= data.len;
			}
		}
		/* update seq num */
		if(seq_num == conn_state->tx_head) {
			pthread_spin_lock(&conn_state->mtx);
			conn_state->time_of_head_change = clock();
			pthread_spin_unlock(&conn_state->mtx);
		}
		clock_t time_elapsed_msec =
		  ((clock() - conn_state->time_of_head_change) * 1000) / CLOCKS_PER_SEC;
		if(time_elapsed_msec >= TCP_TIMEOUT) {
			conn_state->seq_num = conn_state->tx_head;
			conn_state->tx_buf_mgr->reset();
			break;
		} else {
			conn_state->seq_num += data.len;
		}
		if(!data.ptr || amount == 0)
			break;
		if(count++ > 10 && amount == 0)
			break;
	}
}

int tcp_send(std::shared_ptr<tcp_endpoint> endp,
  nstack_queue_entry *nqe,
  char *payload,
  uint16_t payload_size)
{
	// tcp_conn_t conn = tcp_client_table_get(client_id);
	// assert(conn.local.port != 0 && conn.remote.port != 0);
	// tcp_conn_state_t *conn_state = conn_table_get(conn);
	// assert(conn_state != NULL);
	endp->conn_state.tx_buf_mgr->append(endp->client, nqe, payload, payload_size);
	__try_send(endp->conn, &endp->conn_state, payload_size);
	return 0;
	/*
	int ret = char_ring_buffer_add(conn_state->tx_buffer, payload, payload_size);
	if(ret == payload_size) {
	    return 0;
	} else {
	    return ETCPSENDFAILED;
	}
	*/
}

void handle_tcp_send()
{
	fprintf(stderr, "Started TCP send thread\n");

	srand(time(NULL));

	while(true) {
		usleep(100000);

		// fprintf(stderr, "TODO!\n");
		//	tcp_conn_t conn;
		//	tcp_conn_state_t *conn_state = get_next_conn_state(&conn);

		//	if(conn_state == NULL) {
		//		continue;
		//	}

		if(0) {
			// if(conn_state->curr_state == CONN_ESTABLISHED) {
			//	char_ring_buffer_t *tx_buffer = conn_state->tx_buffer;
			//__try_send(conn, conn_state, MSS);
#if 0
			uint32_t seq_num = conn_state->seq_num;
			uint32_t ack_num = conn_state->ack_num;

			auto data = conn_state->tx_buf_mgr->get_next(MSS);
			if(data.ptr) {
				fprintf(stderr, "SEND %d bytes\n", data.len);
				int ret = encap_tcp_packet_2(conn.local.ip,
				  conn.remote.ip,
				  conn.local.port,
				  conn.remote.port,
				  DATA_PKT,
				  seq_num,
				  ack_num,
				  (char *)data.ptr,
				  data.len);
				if(ret) {
					fprintf(stderr, " -- Packet Dropped!!\n");
				}
			}
			/* update seq num */
			if(seq_num == conn_state->tx_head) {
				pthread_spin_lock(&conn_state->mtx);
				conn_state->time_of_head_change = clock();
				pthread_spin_unlock(&conn_state->mtx);
			}
			clock_t time_elapsed_msec =
			  ((clock() - conn_state->time_of_head_change) * 1000) / CLOCKS_PER_SEC;
			if(time_elapsed_msec >= TCP_TIMEOUT) {
				conn_state->seq_num = conn_state->tx_head;
				conn_state->tx_buf_mgr->reset();
			} else {
				conn_state->seq_num += data.len;
			}
#endif
#if 0
			uint32_t avail_bytes = occupied_space(tx_buffer, &seq_num);

			uint32_t bytes_to_send = (avail_bytes < MSS) ? avail_bytes : MSS;

			if(bytes_to_send > 0) {
				char *payload = (char *)malloc(sizeof(char) * bytes_to_send);

				uint32_t bytes = char_ring_buffer_get(tx_buffer, payload, &seq_num, bytes_to_send);
				assert(bytes == bytes_to_send);

				/* send TCP packet */
				fprintf(stderr, "Sending TCP packet with payload = %s", payload);
				int p = 10000 * 0.7; /* 70% drop probability */
				int r = rand() % 10000;
				if(r < p && 0) {
					fprintf(stderr, " -- Packet Dropped!!\n");
				} else {
					object_id_t object_id;
					int ret = encap_tcp_packet(object_id,
					  NOOP,
					  conn.local.ip,
					  conn.remote.ip,
					  conn.local.port,
					  conn.remote.port,
					  DATA_PKT,
					  seq_num,
					  ack_num,
					  payload,
					  bytes_to_send);
					if(ret == 0) {
						fprintf(stderr, "\n");
					} else {
						fprintf(stderr, " -- Packet Dropped!!\n");
					}
				}

				free(payload);
			}

			/* update seq num */
			if(seq_num == tx_buffer->head) {
				pthread_spin_lock(&conn_state->mtx);
				tx_buffer->time_of_head_change = clock();
				pthread_spin_unlock(&conn_state->mtx);
			}
			clock_t time_elapsed_msec =
			  ((clock() - tx_buffer->time_of_head_change) * 1000) / CLOCKS_PER_SEC;
			if(time_elapsed_msec >= TCP_TIMEOUT) {
				conn_state->seq_num = tx_buffer->head;
			} else {
				conn_state->seq_num += bytes_to_send;
			}
#endif
		}
	}
}

void handle_tcp_recv(const char *interface_name,
  remote_info_t *remote_info,
  uint16_t local_port,
  char *payload,
  uint16_t payload_size,
  uint32_t seq_num,
  uint32_t ack_num,
  uint8_t ack,
  uint8_t rst,
  uint8_t syn,
  uint8_t fin)
{
	interface_t *interface = get_interface_by_name(interface_name);
	ip_addr_t local_ip = interface->ip;
	ip_addr_t remote_ip = remote_info->remote_ip;
	uint16_t remote_port = remote_info->remote_port;

	tcp_conn_t conn;
	memcpy(conn.local.ip.ip, local_ip.ip, IP_ADDR_SIZE);
	memcpy(conn.remote.ip.ip, remote_ip.ip, IP_ADDR_SIZE);
	conn.local.port = local_port;
	conn.remote.port = remote_port;

	/* if recv a RST packet, close the connection and cleanup state */
	if(rst == 1) {
		fprintf(stderr,
		  "Error: connection [%u.%u.%u.%u, %u.%u.%u.%u, %u, %u] "
		  "reset by peer\n",
		  (conn.local.ip.ip[0] & 0x000000FF),
		  (conn.local.ip.ip[1] & 0x000000FF),
		  (conn.local.ip.ip[2] & 0x000000FF),
		  (conn.local.ip.ip[3] & 0x000000FF),
		  (conn.remote.ip.ip[0] & 0x000000FF),
		  (conn.remote.ip.ip[1] & 0x000000FF),
		  (conn.remote.ip.ip[2] & 0x000000FF),
		  (conn.remote.ip.ip[3] & 0x000000FF),
		  (conn.local.port & 0x0000FFFF),
		  (conn.remote.port & 0x0000FFFF));
		/* TODO */
		// cleanup_tcp_conn(conn);
		return;
	}

	std::shared_ptr<tcp_endpoint> endp = nullptr;
	{
		std::lock_guard<std::mutex> _lg(connections_lock);
		auto it = connections.find(conn);
		if(it != connections.end())
			endp = it->second;
	}
	/* if recv a non-SYN pkt for an unknown connection, send back RST packet */
	if(syn != 1 && endp == nullptr) {
		fprintf(stderr,
		  "Error: received packet for an unknown connection "
		  "[%u.%u.%u.%u, %u.%u.%u.%u, %u, %u]; sending RST\n",
		  (conn.local.ip.ip[0] & 0x000000FF),
		  (conn.local.ip.ip[1] & 0x000000FF),
		  (conn.local.ip.ip[2] & 0x000000FF),
		  (conn.local.ip.ip[3] & 0x000000FF),
		  (conn.remote.ip.ip[0] & 0x000000FF),
		  (conn.remote.ip.ip[1] & 0x000000FF),
		  (conn.remote.ip.ip[2] & 0x000000FF),
		  (conn.remote.ip.ip[3] & 0x000000FF),
		  (conn.local.port & 0x0000FFFF),
		  (conn.remote.port & 0x0000FFFF));
		int ret = encap_tcp_packet_2(
		  conn.local.ip, conn.remote.ip, conn.local.port, conn.remote.port, RST_PKT, 0, 0, NULL, 0);
		/* TODO: err ?*/
		return;
	}

	if(syn == 1 && ack == 0) { /* received SYN packet */
		half_conn_t local;
		memcpy(local.ip.ip, local_ip.ip, IP_ADDR_SIZE);
		local.port = local_port;

		outstanding_conn_t conn;
		conn.is_valid = true;
		memcpy(conn.remote.ip.ip, remote_ip.ip, IP_ADDR_SIZE);
		conn.remote.port = remote_port;
		conn.seq_num = seq_num;

		std::lock_guard<std::mutex> _lg(listeners_lock);
		auto listener = listeners.find(local);
		if(listener != listeners.end()) {
			listener->second->accept(conn);
		} else {
			/* TODO: is this right? */
			int ret = encap_tcp_packet_2(
			  local.ip, conn.remote.ip, local.port, conn.remote.port, RST_PKT, 0, 0, NULL, 0);
			// outstanding_conns_enqueue(local, conn); // TODO can result in SYN attack
		}

	} else if(syn == 1 && ack == 1) { /* received SYN-ACK packet */
		if(endp->conn_state.curr_state == SYN_SENT) {
			pthread_spin_lock(&endp->conn_state.mtx);
			endp->conn_state.curr_state = SYN_ACK_RECVD;
			endp->conn_state.seq_num = ack_num;
			pthread_spin_unlock(&endp->conn_state.mtx);
			endp->conn_state.ack_num = seq_num + 1;

			encap_tcp_packet_2(conn.local.ip,
			  conn.remote.ip,
			  conn.local.port,
			  conn.remote.port,
			  ACK_PKT,
			  endp->conn_state.seq_num,
			  endp->conn_state.ack_num,
			  NULL,
			  0);

			pthread_spin_lock(&endp->conn_state.mtx);
			endp->conn_state.curr_state = CONN_ESTABLISHED;
			pthread_spin_unlock(&endp->conn_state.mtx);
			fprintf(stderr, "!!!! WE SHOULD TELL THE CLIENT (TODO)\n");
			auto con = endp->client->get_connection(endp->client_cid);
			if(con) {
				con->complete_connection();
			}
		}

	} else if(ack == 1) { /* received ACK or Data packet */
		if(endp->conn_state.curr_state == SYN_ACK_SENT) {
			pthread_spin_lock(&endp->conn_state.mtx);
			endp->conn_state.curr_state = CONN_ESTABLISHED;
			endp->conn_state.seq_num = ack_num;
			pthread_spin_unlock(&endp->conn_state.mtx);
			fprintf(stderr, "!!!!! WE SHOULD TELL THE CLIENT (2) (TODO)\n");
			auto con = endp->client->get_connection(endp->client_cid);
			if(con) {
				con->notify_accept();
			}
		}
		if(endp->conn_state.curr_state == CONN_ESTABLISHED) {
			if(ack_num > endp->conn_state.tx_head) {
				uint32_t bytes = ack_num - endp->conn_state.tx_head;
				// fprintf(stderr, "ACK %d bytes\n", bytes);
				endp->conn_state.tx_buf_mgr->remove(bytes);
				// uint32_t removed_bytes =
				//  char_ring_buffer_remove(conn_state->tx_buffer, NULL, bytes);
				// assert(removed_bytes == bytes);
				endp->conn_state.tx_head += bytes;
				if(bytes > 0) { // TODO: redundant?
					pthread_spin_lock(&endp->conn_state.mtx);
					endp->conn_state.time_of_head_change = clock();
					pthread_spin_unlock(&endp->conn_state.mtx);
				}
			}

			if(endp->conn_state.ack_num == seq_num) {
				uint32_t added_bytes =
				  char_ring_buffer_add(endp->conn_state.rx_buffer, payload, payload_size);
				endp->conn_state.ack_num += added_bytes;

				/* send ACK packet */
				if(added_bytes > 0) {
					int ret = encap_tcp_packet_2(conn.local.ip,
					  conn.remote.ip,
					  conn.local.port,
					  conn.remote.port,
					  ACK_PKT,
					  endp->conn_state.seq_num,
					  endp->conn_state.ack_num,
					  NULL,
					  0);
				}
			}
		}
	}
}

int tcp_recv(std::shared_ptr<tcp_endpoint> endp, char *buffer, uint16_t buffer_size)
{
	return char_ring_buffer_remove(endp->conn_state.rx_buffer, buffer, buffer_size);
}

void cleanup_tcp_conn(tcp_conn_t conn)
{
#if 0
	/* cleanup entry from outstanding_conns table (for server) */
	outstanding_conns_delete(conn.local);

	/* cleanup entry from tcp client table */
	tcp_conn_state_t *conn_state = conn_table_get(conn);
	assert(conn_state != NULL);
	tcp_client_table_delete(conn_state->client_id);

	/* free local TCP port */
	free_tcp_port(conn.local.port);

	/* cleanup entry from conn table */
	conn_table_delete(conn);
#endif
}
