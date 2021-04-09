#ifndef __ENCAPSULATE_H__
#define __ENCAPSULATE_H__

#include "common.h"
#include "tcp_conn.h"

void encap_arp_packet(uint16_t opcode, mac_addr_t dst_mac, ip_addr_t dst_ip);

int encap_udp_packet(object_id_t object_id,
  uint8_t twz_op,
  ip_addr_t src_ip,
  ip_addr_t dst_ip,
  uint16_t src_port,
  uint16_t dst_port,
  char *payload,
  uint16_t payload_size);

int encap_tcp_packet(object_id_t object_id,
  uint8_t twz_op,
  ip_addr_t src_ip,
  ip_addr_t dst_ip,
  uint16_t src_port,
  uint16_t dst_port,
  tcp_pkt_type_t tcp_pkt_type,
  uint32_t seq_num,
  uint32_t ack_num,
  char *payload,
  uint16_t payload_size);
int encap_tcp_packet_2(ip_addr_t src_ip,
  ip_addr_t dst_ip,
  uint16_t src_port,
  uint16_t dst_port,
  tcp_pkt_type_t tcp_pkt_type,
  uint32_t seq_num,
  uint32_t ack_num,
  char *payload,
  uint16_t payload_size);

#endif
