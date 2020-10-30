#include "udp.h"


void udp_tx(uint16_t src_port,
            uint16_t dst_port,
            void* pkt_ptr,
            int pkt_size)
{
    udp_hdr_t* udp_hdr = (udp_hdr_t *)pkt_ptr;

    udp_hdr->src_port = htons(src_port);

    udp_hdr->dst_port = htons(dst_port);

    udp_hdr->len = htons(pkt_size);

    udp_hdr->checksum = 0;
}


void udp_rx(remote_info_t* remote_info,
            void* pkt_ptr)
{
    udp_hdr_t* udp_hdr = (udp_hdr_t *)pkt_ptr;
    uint16_t src_port = ntohs(udp_hdr->src_port);
    uint16_t dst_port = ntohs(udp_hdr->dst_port);

    remote_info->remote_port = src_port;

    char* payload = (char *)pkt_ptr;
    payload += UDP_HDR_SIZE;

    fprintf(stdout, "Received UDP packet from (%d.%d.%d.%d port %d) payload = ",
            remote_info->remote_ip.ip[0],
            remote_info->remote_ip.ip[1],
            remote_info->remote_ip.ip[2],
            remote_info->remote_ip.ip[3],
            src_port);
    fprintf(stdout, "%s\n", payload);
}
