#include "udp.h"

#include "generic_ring_buffer.h"
#include "interface.h"
#include "port.h"
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
    interface_t* interface = get_interface_by_name(interface_name);

    udp_hdr_t* udp_hdr = (udp_hdr_t *)udp_pkt_ptr;

    uint16_t src_port = ntohs(udp_hdr->src_port);
    uint16_t dst_port = ntohs(udp_hdr->dst_port);

    remote_info->remote_port = src_port;

    char* payload = (char *)udp_pkt_ptr;
    payload += UDP_HDR_SIZE;

    if (remote_info->twz_op != NOOP) {
        twz_op_recv(interface_name, remote_info, payload);

    } else {
        udp_port_t* udp_port = get_udp_port(dst_port);
        if (udp_port != NULL) {
            if (compare_ip_addr(udp_port->ip,
                                string_to_ip_addr(DEFAULT_IP),
                                string_to_ip_addr(DEFAULT_IP))
            || compare_ip_addr(udp_port->ip,
                               interface->ip,
                               interface->bcast_ip)) {
                generic_ring_buffer_add(udp_port->rx_buffer, payload);

            } else {
                fprintf(stderr, "Error udp_rx: local ip %u.%u.%u.%u not bound "
                        "to local udp port %u\n",
                        (interface->ip.ip[0] & 0x000000FF),
                        (interface->ip.ip[1] & 0x000000FF),
                        (interface->ip.ip[2] & 0x000000FF),
                        (interface->ip.ip[3] & 0x000000FF),
                        (dst_port & 0x0000FFFF));
            }

        } else {
            fprintf(stderr, "Error udp_rx: local port %u not in use; "
                    "packet dropped\n", (dst_port & 0x0000FFFF));
        }
    }
}
