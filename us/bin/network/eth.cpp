#include "eth.h"

#include "interface.h"
#include "twz.h"
#include "arp.h"
#include "ipv4.h"


void eth_tx(const char* interface_name,
            mac_addr_t dst_mac,
            uint16_t eth_type,
            void *pkt_ptr,
            int pkt_size)
{
    interface_t* interface = get_interface_by_name(interface_name);

    eth_hdr_t* eth_hdr = (eth_hdr_t *)pkt_ptr;

    /* source MAC */
    memcpy(eth_hdr->src_mac.mac, interface->mac.mac, MAC_ADDR_SIZE);

    /* destination MAC */
    memcpy(eth_hdr->dst_mac.mac, dst_mac.mac, MAC_ADDR_SIZE);

    /* Ethernet type */
    eth_hdr->type = htons(eth_type);

    /* create a pointer to the packet */
    struct packet_queue_entry pqe;
    pqe.qe.info = get_id();
    pqe.ptr = twz_ptr_swizzle(&interface->tx_queue_obj, pkt_ptr, FE_READ);
    pqe.len = pkt_size;

    /* enqueue packet (pointer) to primary tx queue */
    queue_submit(&interface->tx_queue_obj, (struct queue_entry *)&pqe, 0);

    /* for debugging */
    //fprintf(stdout, "[debug] Tx ETH Frame: ");
    //for (int i = 0; i < pkt_size; ++i) {
    //    fprintf(stdout, "%02X ", *((uint8_t *)pkt_ptr + i));
    //}
    //fprintf(stdout, "\n");
}


void eth_rx(const char* interface_name)
{
    interface_t* interface = get_interface_by_name(interface_name);
    twzobj* rx_queue_obj = &interface->rx_queue_obj;

    fprintf(stdout, "Started the packet receive thread for interface %s\n",
            interface_name);

    while (true) {
        /* store pointer to received packet */
        struct packet_queue_entry pqe;

        /* dequeue packet (pointer) from primary rx queue */
        queue_receive(rx_queue_obj, (struct queue_entry *)&pqe, 0);

#ifdef LOOPBACK_TESTING
        char* ph = (char *)twz_object_lea(rx_queue_obj, pqe.ptr);
#else
        /* packet structure from the nic starts with a packet_header struct that
         * contains information followed by the actual packet data. */
        struct packet_header* ph = (struct packet_header *)
            twz_object_lea(rx_queue_obj, pqe.ptr);
        ph += 1;
#endif

        /* decapsulate then send to higher layers */
        eth_hdr_t* pkt_ptr = (eth_hdr_t *)ph;
        mac_addr_t src_mac = pkt_ptr->src_mac;
        mac_addr_t dst_mac = pkt_ptr->dst_mac;
        uint16_t eth_type = ntohs(pkt_ptr->type);

        if (compare_mac_addr(interface->mac, dst_mac) == false) {
            fprintf(stderr, "eth_rx: wrong destination; packet dropped (");
            fprintf(stderr, "SRC: %02X:%02X:%02X:%02X:%02X:%02X ",
                    src_mac.mac[0], src_mac.mac[1], src_mac.mac[2], src_mac.mac[3],
                    src_mac.mac[4], src_mac.mac[5]);
            fprintf(stderr, "DST: %02X:%02X:%02X:%02X:%02X:%02X ",
                    dst_mac.mac[0], dst_mac.mac[1], dst_mac.mac[2], dst_mac.mac[3],
                    dst_mac.mac[4], dst_mac.mac[5]);
            fprintf(stderr, "TYPE: 0x%04X)\n", eth_type);

        } else {
            char* payload = (char *)pkt_ptr;
            payload += ETH_HDR_SIZE;

            remote_info_t remote_info;
            remote_info->twz_op = NOOP;

            switch (eth_type) {
                case TWIZZLER:
                    twz_rx(interface_name, &remote_info, payload);
                    break;

                case ARP:
                    arp_rx(interface_name, payload);
                    break;

                case IPV4:
                    ip_rx(interface_name, &remote_info, payload);
                    break;

                default:
                    fprintf(stderr, "eth_rx: unrecognized Ethernet type 0x%04X; "
                            "packet dropped\n", eth_type);
            }
        }

        /* enqueue an entry in the completion rx queue to signal rx is complete
         * so the packet memory can be freed */
        queue_complete(rx_queue_obj, (struct queue_entry *)&pqe, 0);
    }
}
