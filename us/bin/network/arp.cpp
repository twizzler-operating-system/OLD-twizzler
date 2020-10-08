#include "arp.h"

#include "interface.h"
#include "send.h"


static std::map<uint8_t*,uint8_t*> arp_table;
static std::mutex arp_table_mutex;


void arp_table_insert(uint8_t* proto_addr,
                    uint8_t* hw_addr)
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    arp_table_mutex.lock();
    bool found;
    for (it = arp_table.begin(); it != arp_table.end(); ++it) {
        found = true;
        for (int i = 0; i < PROTO_ADDR_SIZE; ++i) {
            if (it->first[i] != proto_addr[i]) {
                found = false;
                break;
            }
        }
        if (found) break;
    }
    if (!found) {
        arp_table[proto_addr] = hw_addr;
    } else {
        it->second = hw_addr;
    }
    arp_table_mutex.unlock();
}


uint8_t* arp_table_get(uint8_t* proto_addr)
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    arp_table_mutex.lock();
    bool found;
    for (it = arp_table.begin(); it != arp_table.end(); ++it) {
        found = true;
        for (int i = 0; i < PROTO_ADDR_SIZE; ++i) {
            if (it->first[i] != proto_addr[i]) {
                found = false;
                break;
            }
        }
        if (found) break;
    }
    if (!found) {
        arp_table_mutex.unlock();
        return NULL;
    } else {
        arp_table_mutex.unlock();
        return it->second;
    }
}


void arp_table_delete(uint8_t* proto_addr)
{
    arp_table_mutex.lock();
    arp_table.erase(proto_addr);
    arp_table_mutex.unlock();
}


void arp_table_view()
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    arp_table_mutex.lock();
    fprintf(stdout, "ARP Table:\n");
    fprintf(stdout, "------------------------------------------\n");
    for (it = arp_table.begin(); it != arp_table.end(); ++it) {
        int i;
        for (i = 0; i < PROTO_ADDR_SIZE-1; ++i) {
            fprintf(stdout, "%d.", it->first[i]);
        }
        fprintf(stdout, "%d -> ", it->first[i]);

        for (i = 0; i < HW_ADDR_SIZE-1; ++i) {
            fprintf(stdout, "%02X:", it->second[i]);
        }
        fprintf(stdout, "%02X\n", it->second[i]);
    }
    fprintf(stdout, "------------------------------------------\n");
    arp_table_mutex.unlock();
}


void arp_table_clear()
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    arp_table_mutex.lock();
    for (it = arp_table.begin(); it != arp_table.end(); ++it) {
        arp_table.erase(it);
    }
    arp_table_mutex.unlock();
}


void arp_tx(uint16_t hw_type,
          uint16_t proto_type,
          uint8_t hw_addr_len,
          uint8_t proto_addr_len,
          uint16_t opcode,
          uint8_t sender_hw_addr[HW_ADDR_SIZE],
          uint8_t sender_proto_addr[PROTO_ADDR_SIZE],
          uint8_t target_hw_addr[HW_ADDR_SIZE],
          uint8_t target_proto_addr[PROTO_ADDR_SIZE],
          void* pkt_ptr)
{
    arp_hdr_t* arp_hdr = (arp_hdr_t *)pkt_ptr;

    arp_hdr->hw_type = hw_type;

    arp_hdr->proto_type = proto_type;

    arp_hdr->hw_addr_len = hw_addr_len;

    arp_hdr->proto_addr_len = proto_addr_len;

    arp_hdr->opcode = opcode;

    memcpy(arp_hdr->sender_hw_addr, sender_hw_addr, HW_ADDR_SIZE);

    memcpy(arp_hdr->sender_proto_addr, sender_proto_addr, PROTO_ADDR_SIZE);

    memcpy(arp_hdr->target_hw_addr, target_hw_addr, HW_ADDR_SIZE);

    memcpy(arp_hdr->target_proto_addr, target_proto_addr, PROTO_ADDR_SIZE);
}


void arp_rx(const char* interface_name,
          void* pkt_ptr)
{
    interface_t* interface = get_interface_by_name(interface_name);

    arp_hdr_t* arp_hdr = (arp_hdr_t *)pkt_ptr;

    switch (arp_hdr->opcode) {
        case ARP_REQUEST:
            arp_table_insert(arp_hdr->sender_proto_addr, arp_hdr->sender_hw_addr);

            for (int i = 0; i < PROTO_ADDR_SIZE; ++i) {
                if (arp_hdr->target_proto_addr[i] != interface->ip.ip[i]) {
                    return;
                }
            }

            mac_addr_t dst_mac;
            ip_addr_t dst_ip;
            memcpy(dst_mac.mac, arp_hdr->sender_hw_addr, HW_ADDR_SIZE);
            memcpy(dst_ip.ip, arp_hdr->sender_proto_addr, PROTO_ADDR_SIZE);

            /* send ARP Reply packet */
            send_arp_packet(interface_name, ARP_REPLY, dst_mac, dst_ip);
            break;

        case ARP_REPLY:
            arp_table_insert(arp_hdr->sender_proto_addr, arp_hdr->sender_hw_addr);
            arp_table_view();
            break;

        default:
            fprintf(stderr, "Error arp_rx: unrecognized opcode\n");
    }
}
