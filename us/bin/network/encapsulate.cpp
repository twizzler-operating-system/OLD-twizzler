#include "encapsulate.h"

#include "interface.h"
#include "eth.h"
#include "twz.h"
#include "arp.h"
#include "ipv4.h"
#include "udp.h"
#include "tcp.h"


static bool arp_check(ip_addr_t dst_ip)
{
    uint8_t* dst_mac_addr;
    if ((dst_mac_addr = arp_table_get(dst_ip.ip)) == NULL) {
        if (!is_arp_req_inflight(dst_ip.ip)) {
            /* send ARP Request */
            mac_addr_t mac = (mac_addr_t) {.mac = {0}};
            encap_arp_packet(ARP_REQUEST, mac, dst_ip);

            insert_arp_req(dst_ip.ip);
        }

        return false;
    }

    return true;
}


void encap_arp_packet(uint16_t opcode,
                      mac_addr_t dst_mac,
                      ip_addr_t dst_ip)
{
    /* find the tx interface */
    char interface_name[MAX_INTERFACE_NAME_SIZE];
    ip_table_get(dst_ip, interface_name);
    if (interface_name == NULL) {
        fprintf(stderr, "Error encap_arp_packet: "
                "ip_table_get returned no valid interface\n");
        exit(1);
    }
    interface_t* interface = get_interface_by_name(interface_name);

    /* calculate the size of packet buffer */
    uint16_t pkt_size = ETH_HDR_SIZE + ARP_HDR_SIZE;

    /* create packet buffer object to store packet that will the transfered */
    void* pkt_ptr = allocate_packet_buffer_object(pkt_size);

    /* add ARP header */
    char* arp_ptr = (char *)pkt_ptr;
    arp_ptr += ETH_HDR_SIZE;
    arp_tx(1, IPV4, HW_ADDR_SIZE, PROTO_ADDR_SIZE, opcode,
            interface->mac.mac, interface->ip.ip,
            dst_mac.mac, dst_ip.ip, arp_ptr);

    /* add Ethernet header */
    mac_addr_t broadcast_mac = (mac_addr_t) {
        .mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
    };
    eth_tx(interface_name, (opcode == ARP_REQUEST) ? broadcast_mac : dst_mac,
            ARP, pkt_ptr, pkt_size);
}


int encap_udp_packet(object_id_t object_id,
                     uint8_t twz_op,
                     ip_addr_t src_ip,
                     ip_addr_t dst_ip,
                     uint16_t src_port,
                     uint16_t dst_port,
                     char* payload,
                     uint16_t payload_size)
{
    clock_t start = clock();
    while (arp_check(dst_ip) == false) {
        usleep(1000);
        clock_t time_elapsed_msec = ((clock() - start) * 1000) / CLOCKS_PER_SEC;
        if (time_elapsed_msec >= ARP_TIMEOUT) {
            return EARPFAILED;
        }
    }

    bool has_twz_hdr = true;
    if (twz_op == NOOP) has_twz_hdr = false;

    /* find the tx interface */
    char interface_name[MAX_INTERFACE_NAME_SIZE];
    ip_addr_t default_ip = string_to_ip_addr(DEFAULT_IP);

    if (compare_ip_addr(src_ip, default_ip, default_ip)) { /* use dst ip to find
                                                              the tx interface */
        ip_table_get(dst_ip, interface_name);
    } else {                                               /* use src ip to find
                                                              the tx interface */
        get_interface_by_ip(src_ip, interface_name);
    }

    if (interface_name == NULL) {
        fprintf(stderr, "Error encap_tcp_packet: "
                "no valid interface found to transmit the packet\n");
        exit(1);
    }

    interface_t* interface = get_interface_by_name(interface_name);

    /* calculate the size of packet buffer */
    uint16_t pkt_size;
    if (payload == NULL) {
        payload_size = 0;
    }
    if (has_twz_hdr) {
        pkt_size = ETH_HDR_SIZE + TWZ_HDR_SIZE + IP_HDR_SIZE
            + UDP_HDR_SIZE + payload_size;
    } else {
        pkt_size = ETH_HDR_SIZE + IP_HDR_SIZE + UDP_HDR_SIZE + payload_size;
    }
    if (pkt_size > MAX_ETH_FRAME_SIZE) {
        return EMAXFRAMESIZE;
    }

    /* create packet buffer object to store packet that will the transfered */
    void* pkt_ptr = allocate_packet_buffer_object(pkt_size);

    /* add payload */
    if (payload != NULL) {
        char* payload_ptr = (char *)pkt_ptr;
        payload_ptr += (pkt_size - payload_size);
        memcpy(payload_ptr, payload, payload_size);
    }

    /* add UDP header */
    char* udp_ptr = (char *)pkt_ptr;
    udp_ptr += (pkt_size - UDP_HDR_SIZE - payload_size);
    udp_tx(src_port, dst_port, udp_ptr, (UDP_HDR_SIZE + payload_size));

    /* add IPv4 header */
    char* ip_ptr = (char *)pkt_ptr;
    ip_ptr += (pkt_size - IP_HDR_SIZE - UDP_HDR_SIZE - payload_size);
    ip_tx(interface_name, dst_ip, UDP, ip_ptr,
            (IP_HDR_SIZE + UDP_HDR_SIZE + payload_size));

    if (has_twz_hdr) {
        /* add Twizzler header */
        char* twz_ptr = (char *)pkt_ptr;
        twz_ptr += (pkt_size - TWZ_HDR_SIZE - IP_HDR_SIZE
                - UDP_HDR_SIZE - payload_size);
        twz_tx(object_id, twz_op, IPV4, twz_ptr);
    }

    /* add Ethernet header */
    uint8_t* dst_mac_addr = arp_table_get(dst_ip.ip);
    mac_addr_t dst_mac;
    memcpy(dst_mac.mac, dst_mac_addr, MAC_ADDR_SIZE);
    eth_tx(interface_name, dst_mac, (has_twz_hdr) ? TWIZZLER : IPV4,
            pkt_ptr, pkt_size);

    return 0;
}


int encap_tcp_packet(object_id_t object_id,
                     uint8_t twz_op,
                     ip_addr_t src_ip,
                     ip_addr_t dst_ip,
                     uint16_t src_port,
                     uint16_t dst_port,
                     tcp_pkt_type_t tcp_pkt_type,
                     uint32_t seq_num,
                     uint32_t ack_num,
                     char* payload,
                     uint16_t payload_size)
{
    clock_t start = clock();
    while (arp_check(dst_ip) == false) {
        usleep(1000);
        clock_t time_elapsed_msec = ((clock() - start) * 1000) / CLOCKS_PER_SEC;
        if (time_elapsed_msec >= ARP_TIMEOUT) {
            return EARPFAILED;
        }
    }

    bool has_twz_hdr = true;
    if (twz_op == NOOP) has_twz_hdr = false;

    /* find the tx interface */
    char interface_name[MAX_INTERFACE_NAME_SIZE];
    ip_addr_t default_ip = string_to_ip_addr(DEFAULT_IP);

    if (compare_ip_addr(src_ip, default_ip, default_ip)) { /* use dst ip to find
                                                              the tx interface */
        ip_table_get(dst_ip, interface_name);
    } else {                                               /* use src ip to find
                                                              the tx interface */
        get_interface_by_ip(src_ip, interface_name);
    }

    if (interface_name == NULL) {
        fprintf(stderr, "Error encap_tcp_packet: "
                "no valid interface found to transmit the packet\n");
        exit(1);
    }

    interface_t* interface = get_interface_by_name(interface_name);

    /* calculate the size of packet buffer */
    uint16_t pkt_size;
    if (payload == NULL) {
        payload_size =0;
    }
    if (has_twz_hdr) {
        pkt_size = ETH_HDR_SIZE + TWZ_HDR_SIZE + IP_HDR_SIZE
            + TCP_HDR_SIZE + payload_size;
    } else {
        pkt_size = ETH_HDR_SIZE + IP_HDR_SIZE + TCP_HDR_SIZE + payload_size;
    }
    if (pkt_size > MAX_ETH_FRAME_SIZE) {
        return EMAXFRAMESIZE;
    }

    /* create packet buffer object to store packet that will the transfered */
    void* pkt_ptr = allocate_packet_buffer_object(pkt_size);

    /* add payload */
    if (payload != NULL) {
        char* payload_ptr = (char *)pkt_ptr;
        payload_ptr += (pkt_size - payload_size);
        memcpy(payload_ptr, payload, payload_size);
    }

    /* add TCP header */
    uint8_t ns = 0, cwr = 0, ece = 0, urg = 0, ack = 0,
            psh = 0, rst = 0, syn = 0, fin = 0;
    switch (tcp_pkt_type) {
        case SYN_PKT:
            syn = 1;
            break;

        case SYN_ACK_PKT:
            ack = 1;
            syn = 1;
            break;

        case ACK_PKT:
            ack = 1;
            break;

        case DATA_PKT:
            ack = 1;
            break;

        case FIN_PKT:
            ack = 1;
            fin = 1;
            break;

        case RST_PKT:
            rst = 1;
            break;

        default:
            fprintf(stderr, "Error encap_tcp_packet: unrecognized tcp_pkt_type\n");
            exit(1);
    }
    char* tcp_ptr = (char *)pkt_ptr;
    tcp_ptr += (pkt_size - TCP_HDR_SIZE - payload_size);
    tcp_tx(interface->ip, dst_ip, src_port, dst_port, seq_num, ack_num,
            ns, cwr, ece, urg, ack, psh, rst, syn, fin, 0, 0, tcp_ptr,
            (TCP_HDR_SIZE + payload_size));

    /* add IPv4 header */
    char* ip_ptr = (char *)pkt_ptr;
    ip_ptr += (pkt_size - IP_HDR_SIZE - TCP_HDR_SIZE - payload_size);
    ip_tx(interface_name, dst_ip, TCP, ip_ptr,
            (IP_HDR_SIZE + TCP_HDR_SIZE + payload_size));

    if (has_twz_hdr) {
        /* add Twizzler header */
        char* twz_ptr = (char *)pkt_ptr;
        twz_ptr += (pkt_size - TWZ_HDR_SIZE - IP_HDR_SIZE
                - TCP_HDR_SIZE - payload_size);
        twz_tx(object_id, twz_op, IPV4, twz_ptr);
    }

    /* add Ethernet header */
    uint8_t* dst_mac_addr = arp_table_get(dst_ip.ip);
    mac_addr_t dst_mac;
    memcpy(dst_mac.mac, dst_mac_addr, MAC_ADDR_SIZE);
    eth_tx(interface_name, dst_mac, (has_twz_hdr) ? TWIZZLER : IPV4,
            pkt_ptr, pkt_size);

    return 0;
}
