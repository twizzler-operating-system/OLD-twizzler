#ifndef __UDP_CONN_H__
#define __UDP_CONN_H__

#include "common.h"
#include "generic_ring_buffer.h"

//error codes
#define EUDPBINDFAILED 40

#define PKT_BUFFER_SIZE 1000 //num of packets

typedef struct udp_port {
    uint16_t client_id;
    ip_addr_t ip;
    generic_ring_buffer_t* rx_buffer;
} udp_port_t;

void udp_client_table_put(uint16_t client_id,
                          uint16_t udp_port);

uint16_t udp_client_table_get(uint16_t client_id);

void udp_client_table_delete(uint16_t client_id);

void udp_client_table_view();

udp_port_t* get_udp_port(uint16_t port);

int bind_to_udp_port(uint16_t client_id,
                     ip_addr_t ip,
                     uint16_t port);

uint16_t bind_to_random_udp_port(uint16_t client_id);

void free_udp_port(uint16_t port);

int udp_send(uint16_t client_id,
             ip_addr_t dst_ip,
             uint16_t dst_port,
             char* payload,
             uint16_t payload_size);

int udp_recv(uint16_t client_id,
             char* buffer,
             uint16_t buffer_size,
             ip_addr_t* remote_ip,
             uint16_t* remote_port);

void cleanup_udp_conn(uint16_t port);

#endif
