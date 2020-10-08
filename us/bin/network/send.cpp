#include "send.h"

#include "interface.h"
#include "eth.h"
#include "ipv4.h"
#include "arp.h"


void send_arp_packet(const char* interface_name,
                   uint16_t opcode,
                   mac_addr_t dst_mac,
                   ip_addr_t dst_ip)
{
    /* calculate the size of packet buffer */
    int pkt_size = ETH_HDR_SIZE + ARP_HDR_SIZE;

    /* create packet buffer object to store packet that will the transfered */
    void* pkt_ptr = allocate_packet_buffer_object(pkt_size);

    /* add ARP header */
    char* arp_ptr = (char *)pkt_ptr;
    arp_ptr += ETH_HDR_SIZE;
    interface_t* interface = get_interface_by_name(interface_name);
    arp_tx(1, IPV4, HW_ADDR_SIZE, PROTO_ADDR_SIZE, opcode,
            interface->mac.mac, interface->ip.ip,
            dst_mac.mac, dst_ip.ip, arp_ptr);

    /* add ethernet header */
    mac_addr_t broadcast_mac = (mac_addr_t) {
        .mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
    };
    eth_tx(interface_name, (opcode == ARP_REQUEST) ? broadcast_mac : dst_mac,
            ARP, pkt_ptr, pkt_size);
}


int send_ipv4_packet(const char* interface_name,
                    ip_addr_t dst_ip,
                    uint8_t ip_type,
                    char* payload)
{
    /* calculate the size of packet buffer */
    uint16_t payload_size = 0;
    if (payload) {
        payload_size = strlen(payload);
    }
    int IP_HDR_SIZE = 20;
    int pkt_size = ETH_HDR_SIZE + IP_HDR_SIZE + payload_size;
    if (pkt_size > MAX_ETH_FRAME_SIZE) {
        fprintf(stderr, "Error encapsulate_mac_ping_packet: "
                "sending frame larger than allowed ethernet standard\n");
        return -1;
    }

    /* create packet buffer object to store packet that will the transfered */
    void* pkt_ptr = allocate_packet_buffer_object(pkt_size);

    /* add payload */
    if (payload) {
        char* payload_ptr = (char *)pkt_ptr;
        payload_ptr += ETH_HDR_SIZE + IP_HDR_SIZE;
        strcpy(payload_ptr, payload);
    }

    /* add ipv4 header */
    char* ip_ptr = (char *)pkt_ptr;
    ip_ptr += ETH_HDR_SIZE;
    ip_tx(interface_name, dst_ip, ip_type, ip_ptr, (pkt_size - ETH_HDR_SIZE));

    /* ARP check */
    uint8_t* dst_mac_addr;
    if ((dst_mac_addr = arp_table_get(dst_ip.ip)) == NULL) {
        /* send ARP Request packet */
        mac_addr_t mac = (mac_addr_t) {.mac = {0}};
        send_arp_packet(interface_name, ARP_REQUEST, mac, dst_ip);

        /* blocking send */
        uint64_t count = 0;
        while ((dst_mac_addr = arp_table_get(dst_ip.ip)) == NULL) {
            usleep(1);
            ++count;
            if (count == ARP_TIMEOUT) {
                return ARP_TIMEOUT_ERROR;
            }
        }
    }

    mac_addr_t dst_mac;
    memcpy(dst_mac.mac, dst_mac_addr, MAC_ADDR_SIZE);
    /* add ethernet header */
    eth_tx(interface_name, dst_mac, IPV4, pkt_ptr, pkt_size);

    return 0;
}
