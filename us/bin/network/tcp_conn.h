#ifndef __TCP_CONN_H__
#define __TCP_CONN_H__

#include "char_ring_buffer.h"
#include "common.h"

// error codes
#define ETCPBINDFAILED 30
#define ECONNFAILED 31
#define ETCPSENDFAILED 32

#define CHAR_RING_BUFFER_SIZE 1500000 // bytes

#define TCP_TIMEOUT 5000 // ms

#define MSS 10 // bytes (default: 536)

// TCP states
typedef enum {
	SYN_SENT,
	SYN_ACK_RECVD,
	SYN_ACK_SENT,
	ACK_RECVD,
	CONN_ESTABLISHED,
} tcp_state_t;

// TCP packet types
typedef enum { SYN_PKT, SYN_ACK_PKT, ACK_PKT, DATA_PKT, FIN_PKT, RST_PKT } tcp_pkt_type_t;

typedef struct half_conn {
	ip_addr_t ip;
	uint16_t port;
} half_conn_t;

typedef struct outstanding_conn {
	bool is_valid;
	half_conn_t remote;
	uint32_t seq_num;
} outstanding_conn_t;

typedef struct tcp_conn {
	half_conn_t local;
	half_conn_t remote;
} tcp_conn_t;

#include "databuf.h"

typedef struct tcp_conn_state {
	pthread_spinlock_t mtx;
	uint16_t client_id;
	tcp_state_t curr_state;
	uint32_t seq_num;
	uint32_t ack_num;

	databuf *rx_buf_mgr;
	databuf *tx_buf_mgr;
	std::mutex *lock;
	uint32_t tx_head;
	clock_t time_of_head_change;

	// char_ring_buffer_t *tx_buffer;
	char_ring_buffer_t *rx_buffer;
} tcp_conn_state_t;

bool compare_half_conn(half_conn_t half_conn_1, half_conn_t half_conn_2);

void outstanding_conns_enqueue(half_conn_t half_conn, outstanding_conn_t outstanding_conn);

outstanding_conn_t outstanding_conns_dequeue(half_conn_t half_conn);

void outstanding_conns_delete(half_conn_t half_conn);

void outstanding_conns_view();

bool compare_tcp_conn(tcp_conn_t conn_1, tcp_conn_t conn_2);

void conn_table_put(tcp_conn_t conn, tcp_conn_state_t *tcp_conn_state);

tcp_conn_state_t *conn_table_get(tcp_conn_t conn);

void conn_table_delete(tcp_conn_t conn);

void conn_table_view();

tcp_conn_state_t *get_next_conn_state(tcp_conn_t *conn);

void bootstrap_conn_table();

void tcp_client_table_put(uint16_t client_id, tcp_conn_t conn);

tcp_conn_t tcp_client_table_get(uint16_t client_id);

void tcp_client_table_delete(uint16_t client_id);

void tcp_client_table_view();

int bind_to_tcp_port(uint16_t client_id, ip_addr_t ip, uint16_t port);

uint16_t bind_to_random_tcp_port();

void free_tcp_port(uint16_t port);

int establish_tcp_conn_client(uint16_t client_id, half_conn_t *local, half_conn_t remote);

int establish_tcp_conn_server(uint16_t client_id, half_conn_t local, half_conn_t *remote);

class net_client;
class nstack_queue_entry;
int tcp_send(uint16_t client_id,
  std::shared_ptr<net_client> client,
  nstack_queue_entry *nqe,
  char *payload,
  uint16_t payload_size);

void handle_tcp_send();

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
  uint8_t fin);

int tcp_recv(uint16_t client_id, char *buffer, uint16_t buffer_size);

void cleanup_tcp_conn(tcp_conn_t conn);

#endif
