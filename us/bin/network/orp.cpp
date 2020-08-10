#include <map>
#include <iostream>

#include "orp.h"

/*helper functions*/
std::string convert_ipv4_to_string(ipv4_addr_t addr)
{
    std::string ip_string = addr.addr;
    return ip_string;
}
std::string convert_mac_to_string(mac_addr_t addr)
{
    std::string mac_string;
    mac_string.assign(addr.mac, addr.mac + MAC_ADDR_SIZE);
    return mac_string;
}
mac_addr_t convert_string_to_mac(std::string addr)
{
    
}

/*ORP APIs*/
void init_orp_map(char *name, twzobj *orp_table_obj)
{
    /*create object*/
    if(twz_object_new(orp_table_obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE) < 0)
    {
        fprintf(stderr, "init_orp_map: Could not create object\n");
        exit(1);
    }
    if(twz_name_assign(twz_object_guid(orp_table_obj), name)<0)
    {
        fprintf(stderr,"init_orp_map: Could not name object\n");
        exit(1);
    }
}


void add_orp_entry(twzobj *orp_table_obj, ipv4_addr_t ip_addr, mac_addr_t mac_addr)
{
    orp_table_t *orp_map = (orp_table_t *)twz_object_base(orp_table_obj);
    std::string ip_string = convert_ipv4_to_string(ip_addr);
    std::string mac_string = convert_mac_to_string(mac_addr);
    (orp_map->orp_lookup_table).insert({ip_string, mac_string});
//
//    for (auto& t : (orp_map->orp_lookup_table))
//    std::cout << t.first << " "
//              << t.second << "\n";

}
void remove_orp_entry(twzobj *orp_table_obj, ipv4_addr_t *addr)
{
    
}
mac_addr_t orp_lookup(twzobj *orp_table_obj, ipv4_addr_t *addr)
{
    
}

