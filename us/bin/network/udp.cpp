#include "udp.h"

#include "twz.h"
#include "twz_op.h"


void udp_tx(uint16_t src_port,
            uint16_t dst_port,
            void* udp_pkt_ptr,
            uint16_t udp_pkt_size)
{
    udp_hdr_t* udp_hdr = (udp_hdr_t *)udp_pkt_ptr;

    udp_hdr->src_port = htons(src_port);

    udp_hdr->dst_port = htons(dst_port);

    udp_hdr->len = htons(udp_pkt_size);

    udp_hdr->checksum = 0;
}


void udp_rx(const char* interface_name,
            remote_info_t* remote_info,
            void* udp_pkt_ptr)
{
    udp_hdr_t* udp_hdr = (udp_hdr_t *)udp_pkt_ptr;

    uint16_t src_port = ntohs(udp_hdr->src_port);
    uint16_t dst_port = ntohs(udp_hdr->dst_port);

    remote_info->remote_port = src_port;

    char* payload = (char *)udp_pkt_ptr;
    payload += UDP_HDR_SIZE;

    if (remote_info->twz_op != NOOP) {
        twz_op_recv(interface_name, remote_info, payload);

    } else {
        fprintf(stderr, "Received UDP packet from ('%u.%u.%u.%u', %u) payload = ",
                (remote_info->remote_ip.ip[0] & 0x000000FF),
                (remote_info->remote_ip.ip[1] & 0x000000FF),
                (remote_info->remote_ip.ip[2] & 0x000000FF),
                (remote_info->remote_ip.ip[3] & 0x000000FF),
                (src_port & 0x0000FFFF));
        fprintf(stderr, "%s\n", payload);
    }
}
