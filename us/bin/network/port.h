#ifndef __PORT_H__
#define __PORT_H__

#include "common.h"
#include "generic_ring_buffer.h"

#define PKT_BUFFER_SIZE 1000 //num of packets

typedef struct udp_port {
    ip_addr_t ip;
    generic_ring_buffer_t* rx_buffer;
} udp_port_t;

udp_port_t* get_udp_port(uint16_t port);

void bind_to_udp_port(ip_addr_t ip,
                      uint16_t port);

uint16_t bind_to_random_udp_port();

void free_udp_port(uint16_t port);

void bind_to_tcp_port(ip_addr_t ip,
                      uint16_t port);

uint16_t bind_to_random_tcp_port();

void free_tcp_port(uint16_t port);

#endif
