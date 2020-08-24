#include <map>
#include <iostream>
#include <unistd.h>

#include "flip.h"


static std::map<std::string, std::string> arp_lookup_table;

/*Helper functions*/
std::string convert_mac_to_string(mac_addr_t addr)
{
    char mac_char_ptr[MAX_MAC_CHAR_SIZE];
    memset(mac_char_ptr, 0, MAX_MAC_CHAR_SIZE);
    
    char t[3];
    
    for(int i = 0; i < 6; i++)
    {
        memset(t, '\0', 3);
        sprintf(t, "%x", addr.mac[i]);
        strcat(mac_char_ptr, t);
    }
    
    std::string mac_string = mac_char_ptr;
    
    return mac_string;
}


mac_addr_t convert_string_to_mac(std::string string_addr)
{
    mac_addr_t mac_addr;
    return mac_addr;
}





/*AddressResolutionProtocol APIs*/
void add_arp_entry(ipv4_addr_t ip_addr, mac_addr_t mac_addr)
{
    std::string ip_string = ip_addr.addr;
    std::string mac_string = convert_mac_to_string(mac_addr);

    arp_lookup_table.insert({ip_string, mac_string});
}


/*Returns MAC adder where first index is NULL, if ARP resolution was not successful*/
mac_addr_t arp_lookup(char* ip_addr, twzobj *interface_obj, twzobj *tx_queue_obj)
{
    mac_addr_t resolved_mac;
    memset(resolved_mac.mac, 0, MAC_ADDR_SIZE);
    
    if(arp_lookup_table.find(ip_addr) == arp_lookup_table.end())
    {
        fprintf(stderr, "MAC NOT found in ARP table, sending ARP request!\n");
        send_arp_request(ip_addr, interface_obj, tx_queue_obj);
        sleep(1); //wait one second for ARP Reply... FUTURE WORK: use RTT as wait time
        if(arp_lookup_table.find(ip_addr) == arp_lookup_table.end())
        {
            fprintf(stderr, "Could not resolve MAC\n");
            resolved_mac.mac[0] = NULL;
        }
        else
        {
            fprintf(stderr, "MAC resolved with ARP\n");
            resolved_mac.mac[0] = NULL;
            //must be chanced to convert MAC to string
            //convert_string_to_mac(arp_lookup_table[ip_addr]);
        }
    }
    else
    {
        fprintf(stderr, "Found MAC in ARP table\n");
        //convert_string_to_mac(arp_lookup_table[ip_addr]);
        
    }
    
    return resolved_mac;
}


void print_arp_table()
{
    fprintf(stderr, "Printing table:\n");
    for (auto& t : arp_lookup_table)
        std::cout << t.first << " "
                  << t.second << "\n";
}



/*ARP Protocol Implementation*/
void send_arp_request(char *ip_addr, twzobj *interface_obj, twzobj *tx_queue_obj)
{
    fprintf(stderr, "In send_arp_request\n");
    flip_send(arp_meta_1_request, arp_meta_2_request, NULL, ARP_TYPE, true, ip_addr, NULL, interface_obj, tx_queue_obj);
    
}
//void send_arp_reply(eth_hdr_t *eth_hdr);
//void recv_arp_request(eth_hdr_t *eth_hdr);
//void recv_arp_reply(eth_hdr_t *eth_hdr);
