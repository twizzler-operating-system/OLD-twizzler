#include "tcp.h"

#include "interface.h"
#include "ipv4.h"
#include "tcp_conn.h"

void tcp_tx(ip_addr_t src_ip,
            ip_addr_t dst_ip,
            uint16_t src_port,
            uint16_t dst_port,
            uint32_t seq_num,
            uint32_t ack_num,
            uint8_t ns,
            uint8_t cwr,
            uint8_t ece,
            uint8_t urg,
            uint8_t ack,
            uint8_t psh,
            uint8_t rst,
            uint8_t syn,
            uint8_t fin,
            uint16_t window_size,
            uint16_t urg_ptr,
            void* tcp_pkt_ptr,
            uint16_t tcp_pkt_size)
{
    tcp_hdr_t* tcp_hdr = (tcp_hdr_t *)tcp_pkt_ptr;

    tcp_hdr->src_port = htons(src_port);

    tcp_hdr->dst_port = htons(dst_port);

    tcp_hdr->seq_num = htonl(seq_num);

    tcp_hdr->ack_num = htonl(ack_num);

    uint16_t data_offset = 0x0050 | ns;

    uint8_t flags = cwr << 7
                | ece << 6
                | urg << 5
                | ack << 4
                | psh << 3
                | rst << 2
                | syn << 1
                | fin;

    tcp_hdr->data_offset_and_flags = htons(data_offset << 8 | flags);

    tcp_hdr->window_size = htons(window_size);

    tcp_hdr->checksum = 0;

    tcp_hdr->urg_ptr = htons(urg_ptr);

    pseudo_ip_hdr_t pseudo_ip_hdr;
    memcpy(pseudo_ip_hdr.src_ip, src_ip.ip, IP_ADDR_SIZE);
    memcpy(pseudo_ip_hdr.dst_ip, dst_ip.ip, IP_ADDR_SIZE);
    pseudo_ip_hdr.reserved = 0;
    pseudo_ip_hdr.protocol = TCP;
    pseudo_ip_hdr.len = htons(tcp_pkt_size);

    char* pseudo_ip = (char *)&pseudo_ip_hdr;
    char* tcp = (char *)tcp_pkt_ptr;
    uint16_t len = PSEUDO_IP_HDR_SIZE + tcp_pkt_size;
    char* hdr = (char *)malloc(sizeof(char)*len);
    memcpy(hdr, pseudo_ip, PSEUDO_IP_HDR_SIZE);
    memcpy(hdr + PSEUDO_IP_HDR_SIZE, tcp, tcp_pkt_size);

    tcp_hdr->checksum = htons(checksum((unsigned char *)hdr, len));
}


void tcp_rx(const char* interface_name,
            remote_info_t* remote_info,
            void* tcp_pkt_ptr)
{
    interface_t* interface = get_interface_by_name(interface_name);

    tcp_hdr_t* tcp_hdr = (tcp_hdr_t *)tcp_pkt_ptr;

    uint16_t src_port = ntohs(tcp_hdr->src_port);
    uint16_t dst_port = ntohs(tcp_hdr->dst_port);
    uint32_t seq_num = ntohl(tcp_hdr->seq_num);
    uint32_t ack_num = ntohl(tcp_hdr->ack_num);
    uint16_t data_offset_and_flags = ntohs(tcp_hdr->data_offset_and_flags);
    uint8_t tcp_hdr_size = ((data_offset_and_flags & 0xF000) >> 12) * 4;
    uint8_t ack = (data_offset_and_flags & 0x0010) >> 4;
    uint8_t rst = (data_offset_and_flags & 0x0004) >> 2;
    uint8_t syn = (data_offset_and_flags & 0x0002) >> 1;
    uint8_t fin = data_offset_and_flags & 0x0001;

    /* verify checksum */
    uint16_t recvd_checksum = ntohs(tcp_hdr->checksum);
    tcp_hdr->checksum = 0;

    pseudo_ip_hdr_t pseudo_ip_hdr;
    memcpy(pseudo_ip_hdr.src_ip, interface->ip.ip, IP_ADDR_SIZE);
    memcpy(pseudo_ip_hdr.dst_ip, remote_info->remote_ip.ip, IP_ADDR_SIZE);
    pseudo_ip_hdr.reserved = 0;
    pseudo_ip_hdr.protocol = TCP;
    pseudo_ip_hdr.len = htons(remote_info->ip_payload_size);

    char* pseudo_ip = (char *)&pseudo_ip_hdr;
    char* tcp = (char *)tcp_pkt_ptr;
    uint16_t len = PSEUDO_IP_HDR_SIZE + remote_info->ip_payload_size;
    char* hdr = (char *)malloc(sizeof(char)*len);
    memcpy(hdr, pseudo_ip, PSEUDO_IP_HDR_SIZE);
    memcpy(hdr + PSEUDO_IP_HDR_SIZE, tcp, remote_info->ip_payload_size);

    uint16_t calculated_checksum = checksum((unsigned char *)hdr, len);
    if (recvd_checksum != calculated_checksum) {
        fprintf(stderr, "tcp_rx: checksum mismatch; packet dropped\n");
        return;
    }

    remote_info->remote_port = src_port;

    char* payload = (char *)tcp_pkt_ptr;
    payload += tcp_hdr_size;

    /* for debugging */
    //fprintf(stderr, "[TCP recv debug] SRC IP: %u.%u.%u.%u DST IP: %u.%u.%u.%u "
    //        "SRC PORT: %u DST PORT: %u PAYLOAD: %s SIZE: %u SEQ NUM: %u "
    //        "ACK NUM: %u ACK: %d RST: %d SYN: %d FIN: %d\n",
    //        (remote_info->remote_ip.ip[0] & 0x000000FF),
    //        (remote_info->remote_ip.ip[1] & 0x000000FF),
    //        (remote_info->remote_ip.ip[2] & 0x000000FF),
    //        (remote_info->remote_ip.ip[3] & 0x000000FF),
    //        (interface->ip.ip[0] & 0x000000FF),
    //        (interface->ip.ip[1] & 0x000000FF),
    //        (interface->ip.ip[2] & 0x000000FF),
    //        (interface->ip.ip[3] & 0x000000FF),
    //        (src_port & 0x0000FFFF),
    //        (dst_port & 0x0000FFFF),
    //        payload,
    //        ((remote_info->ip_payload_size - tcp_hdr_size) & 0x0000FFFF),
    //        seq_num, ack_num, ack, rst, syn, fin);

    recv_tcp_data(interface_name, remote_info, dst_port, payload,
            (remote_info->ip_payload_size - tcp_hdr_size),
            seq_num, ack_num, ack, rst, syn, fin);
}
