#include "ipv4.h"

#include "interface.h"
#include "arp.h"
#include "udp.h"
#include "tcp.h"


std::vector<ip_table_entry_t> ip_table;
std::mutex ip_table_mtx;


typedef enum {
    START,
    ONE,
    ZERO,
    REJECT
} state_t;


static bool is_netmask_valid(ip_addr_t netmask)
{
    state_t curr_state = START;

    for (int i = 0; i < IP_ADDR_SIZE; ++i) {
        uint8_t byte = netmask.ip[i];
        for (int j = 7; j >= 0; --j) {
            uint8_t bit = (byte & (0x01 << j)) >> j;
            switch (bit) {
                case 0:
                    if (curr_state == START) curr_state = ZERO;
                    else if (curr_state == ONE) curr_state = ZERO;
                    else if (curr_state == ZERO) curr_state = ZERO;
                    break;

                case 1:
                    if (curr_state == START) curr_state = ONE;
                    else if (curr_state == ONE) curr_state = ONE;
                    else if (curr_state == ZERO) curr_state = REJECT;
                    break;
            }
            if (curr_state == REJECT) {
                return false;
            }
        }
    }

    return true;
}


static bool compare_ip_table_entry(ip_table_entry_t entry1,
                                   ip_table_entry_t entry2)
{
    for (int i = 0; i < IP_ADDR_SIZE; ++i) {
        if (entry1.dst_ip.ip[i] != entry2.dst_ip.ip[i]) {
            return false;
        }

        if (entry1.netmask.ip[i] != entry2.netmask.ip[i]) {
            return false;
        }

        if (entry1.gateway.ip[i] != entry2.gateway.ip[i]) {
            return false;
        }
    }

    return !strcmp(entry1.tx_interface_name, entry2.tx_interface_name);
}


void ip_table_put(ip_table_entry_t entry)
{
    if (!is_netmask_valid(entry.netmask)) {
        fprintf(stderr, "Error ip_table_put: invalid netmask\n");
        exit(1);
    }

    std::vector<ip_table_entry_t>:: iterator it;

    ip_table_mtx.lock();

    bool found = false;
    for (it = ip_table.begin(); it != ip_table.end(); ++it) {
        if (compare_ip_table_entry(*it, entry)) {
            found = true;
            break;
        }
    }

    if (!found) {
        ip_table.push_back(entry);
    }

    ip_table_mtx.unlock();
}


static uint8_t count_ones(ip_addr_t netmask)
{
    uint8_t count = 0;
    for (int i = 0; i < IP_ADDR_SIZE; ++i) {
        uint8_t byte = netmask.ip[i];
        for (int j = 7; j >= 0; --j) {
            uint8_t bit = (byte & (0x01 << j)) >> j;
            count += bit;
        }
    }

    assert(count < IP_ADDR_SIZE * 8);
    return count;
}


void ip_table_get(ip_addr_t dst_ip,
                  char* tx_interface_name)
{
    char* lpm = NULL;
    int lpl = -1;

    std::vector<ip_table_entry_t>:: iterator it;

    ip_table_mtx.lock();

    for (it = ip_table.begin(); it != ip_table.end(); ++it) {
        uint8_t net1[IP_ADDR_SIZE];
        uint8_t net2[IP_ADDR_SIZE];

        int count = 0;
        for (int i = 0; i < IP_ADDR_SIZE; ++i) {
            net1[i] = it->dst_ip.ip[i] & it->netmask.ip[i];
            net2[i] = dst_ip.ip[i] & it->netmask.ip[i];
            if (net1[i] == net2[i]) ++count;
            else break;
        }
        if (count == IP_ADDR_SIZE) {
            if (count_ones(it->netmask) > lpl) {
                lpl = count_ones(it->netmask);
                lpm = it->tx_interface_name;
            }
        }
    }

    if (tx_interface_name != NULL) {
        if (lpm != NULL) {
            strncpy(tx_interface_name, lpm, MAX_INTERFACE_NAME_SIZE);
        } else {
            tx_interface_name = NULL;
        }
    }

    ip_table_mtx.unlock();
}


void ip_table_delete(ip_table_entry_t entry)
{
    std::vector<ip_table_entry_t>:: iterator it;

    ip_table_mtx.lock();

    bool found = false;
    for (it = ip_table.begin(); it != ip_table.end(); ++it) {
        if (compare_ip_table_entry(*it, entry)) {
            found = true;
            break;
        }
    }

    if (found) {
        ip_table.erase(it);
    }

    ip_table_mtx.unlock();
}


void ip_table_view()
{
    std::vector<ip_table_entry_t>:: iterator it;

    ip_table_mtx.lock();

    fprintf(stderr, "IP TABLE\n");
    fprintf(stderr, "------------------------------------------\n");
    for (it = ip_table.begin(); it != ip_table.end(); ++it) {
        fprintf(stderr, "%u.%u.%u.%u  %u.%u.%u.%u  %u.%u.%u.%u  %s\n",
                ((*it).dst_ip.ip[0] & 0x000000FF),
                ((*it).dst_ip.ip[1] & 0x000000FF),
                ((*it).dst_ip.ip[2] & 0x000000FF),
                ((*it).dst_ip.ip[3] & 0x000000FF),
                ((*it).netmask.ip[0] & 0x000000FF),
                ((*it).netmask.ip[1] & 0x000000FF),
                ((*it).netmask.ip[2] & 0x000000FF),
                ((*it).netmask.ip[3] & 0x000000FF),
                ((*it).gateway.ip[0] & 0x000000FF),
                ((*it).gateway.ip[1] & 0x000000FF),
                ((*it).gateway.ip[2] & 0x000000FF),
                ((*it).gateway.ip[3] & 0x000000FF),
                (*it).tx_interface_name);
    }
    fprintf(stderr, "------------------------------------------\n");

    ip_table_mtx.unlock();
}


void ip_table_clear()
{
    ip_table_mtx.lock();
    ip_table.clear();
    ip_table_mtx.unlock();
}


void ip_tx(const char* interface_name,
           ip_addr_t dst_ip,
           uint8_t ip_type,
           void* ip_pkt_ptr,
           uint16_t ip_pkt_size)
{
    interface_t* interface = get_interface_by_name(interface_name);

    ip_hdr_t* ip_hdr = (ip_hdr_t *)ip_pkt_ptr;

    /* version and header length
     * version is 4 for IPv4
     * header length is assumed to be min ip header len (5 or 20bytes) */
    ip_hdr->ver_and_ihl = 0x45;

    /* type of service; set to 0 i.e. best effort */
    ip_hdr->tos = 0;

    /* total packet length */
    ip_hdr->tot_len = htons(ip_pkt_size);

    /* fragmentation attributes */
    ip_hdr->identification = 0;
    ip_hdr->flags_and_offset = 0b010 << 13; //flag:010 (don't fragment); offset:0

    /* time to live */
    ip_hdr->ttl = 255; //default value

    /* payload protocol */
    ip_hdr->protocol = ip_type;

    /* header checksum */
    ip_hdr->hdr_checksum = 0; //is updated at the end

    /* source IP */
    memcpy(ip_hdr->src_ip.ip, interface->ip.ip, IP_ADDR_SIZE);

    /* destination IP */
    memcpy(ip_hdr->dst_ip.ip, dst_ip.ip, IP_ADDR_SIZE);

    uint8_t ihl = (ip_hdr->ver_and_ihl & 0b00001111) * 4; //in bytes
    ip_hdr->hdr_checksum = htons(checksum((unsigned char *)ip_pkt_ptr, ihl));
}


void ip_rx(const char* interface_name,
           remote_info_t* remote_info,
           void* ip_pkt_ptr)
{
    interface_t* interface = get_interface_by_name(interface_name);

    ip_hdr_t* ip_hdr = (ip_hdr_t *)ip_pkt_ptr;

    if (!compare_ip_addr(ip_hdr->dst_ip, interface->ip, interface->bcast_ip)) {
        fprintf(stderr, "ip_rx: wrong IPv4 destination (%u.%u.%u.%u); "
                "packet dropped\n", (ip_hdr->dst_ip.ip[0] & 0x000000FF),
                (ip_hdr->dst_ip.ip[1] & 0x000000FF),
                (ip_hdr->dst_ip.ip[2] & 0x000000FF),
                (ip_hdr->dst_ip.ip[3] & 0x000000FF));
        return;
    }

    /* verify header checksum */
    uint16_t recvd_checksum = ntohs(ip_hdr->hdr_checksum);
    ip_hdr->hdr_checksum = 0;

    uint8_t ihl = (ip_hdr->ver_and_ihl & 0b00001111) * 4; //in bytes
    uint16_t calculated_checksum = checksum((unsigned char *)ip_hdr, ihl);
    if (recvd_checksum != calculated_checksum) {
        fprintf(stderr, "ip_rx: checksum mismatch; packet dropped\n");
        return;
    }

    remote_info->remote_ip = ip_hdr->src_ip;
    remote_info->ip_payload_size = ntohs(ip_hdr->tot_len) - ihl;

    /* update ARP table entry */
    arp_table_put(remote_info->remote_ip.ip, remote_info->remote_mac.mac);

    char* payload = (char *)ip_pkt_ptr;
    payload += ihl;

    switch (ip_hdr->protocol) {
        case UDP:
            udp_rx(interface_name, remote_info, payload);
            break;

        case TCP:
            tcp_rx(interface_name, remote_info, payload);
            break;

        default:
            fprintf(stderr, "ip_rx: unrecognized IPv4 type 0x%02X; "
                    "packet dropped\n", ip_hdr->protocol);
    }
}
