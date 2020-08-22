#include <map>
#include <iostream>

#include "flip.h"


static std::map<std::string, std::string> arp_lookup_table;

/*helper functions*/
std::string convert_ipv4_to_string(ipv4_addr_t addr)
{
    std::string ip_string = addr.addr;
    return ip_string;
}
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


void add_arp_entry(ipv4_addr_t ip_addr, mac_addr_t mac_addr)
{
    std::string ip_string = convert_ipv4_to_string(ip_addr);
    std::string mac_string = convert_mac_to_string(mac_addr);

    arp_lookup_table.insert({ip_string, mac_string});
}


void print_arp_table()
{
    fprintf(stderr, "Printing table:\n");
    for (auto& t : arp_lookup_table)
        std::cout << t.first << " "
                  << t.second << "\n";
}

