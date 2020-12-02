#include "interface.h"

#include "arp.h"


static std::map<const char*,interface_t*> interface_list;


void init_interface(const char* interface_name,
                    const char* info_queue_name,
                    ip_addr_t interface_ip,
                    ip_addr_t interface_bcast_ip)
{
    twzobj interface_obj;
    twzobj info_obj;

    /* create object to hold interface properties */
    if (twz_object_new(&interface_obj, NULL, NULL,
    TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE) < 0) {
        fprintf(stderr, "Error init_interface: "
                "could not create object for interface properties\n");
        exit(1);
    }

    if (twz_name_assign(twz_object_guid(&interface_obj), interface_name) < 0) {
        fprintf(stderr, "Error init_interface: could not name interface object\n");
        exit(1);
    }

    /* open info object to get physical information of interface */
    if ((twz_object_init_name(&info_obj, info_queue_name, FE_READ | FE_WRITE))) {
        fprintf(stderr, "Error init_interface: failed to open info-queue\n");
        exit(1);
    }
    struct nic_header* nh = (struct nic_header *)twz_object_base(&info_obj);
    uint64_t flags = nh->flags;
    while (!(flags & NIC_FL_MAC_VALID)) {
        /* if it's not set, sleep and specify the comparison value
         * as the value we just checked. */
        twz_thread_sync(THREAD_SYNC_SLEEP, &nh->flags, flags, /* timeout */ NULL);
        /* we woke up, so someone woke us up. Reload the flags to
         * check the new value */
        flags = nh->flags;
        /* we have to go around the loop again because we might
         * have had a spurious wake up. */
    }

    /* assign inferface properties */
    interface_t* interface = (interface_t *)twz_object_base(&interface_obj);
    if (interface_list.insert
            (std::make_pair(interface_name, interface)).second == false) {
        fprintf(stderr, "Error init_interface: interface %s already exists\n",
                interface_name);
        exit(1);
    }
    strncpy(interface->name, interface_name, MAX_INTERFACE_NAME_SIZE);
    memcpy(interface->mac.mac, nh->mac, MAC_ADDR_SIZE);
    memcpy(interface->ip.ip, interface_ip.ip, IP_ADDR_SIZE);
    memcpy(interface->bcast_ip.ip, interface_bcast_ip.ip, IP_ADDR_SIZE);
    /* initialize tx queue obj */
    char tx_queue_name[MAX_INTERFACE_NAME_SIZE];
    strcpy(tx_queue_name, interface->name);
    strcat(tx_queue_name, "-txqueue");
    if (twz_object_init_name(&interface->tx_queue_obj,
    tx_queue_name, FE_READ | FE_WRITE) < 0) {
        fprintf(stderr, "Error init_interface: could not init tx-queue\n");
        exit(1);
    }
    /* initialize rx queue obj */
    char rx_queue_name[MAX_INTERFACE_NAME_SIZE];
    strcpy(rx_queue_name, interface->name);
    strcat(rx_queue_name, "-rxqueue");
    if (twz_object_init_name(&interface->rx_queue_obj,
    rx_queue_name, FE_READ | FE_WRITE) < 0) {
        fprintf(stderr,"Error init_interface: could not init rx-queue\n");
        exit(1);
    }
    /* initialize ARP table */
    uint8_t bcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    arp_table_put(interface->bcast_ip.ip, bcast_mac);
    arp_table_put(interface->ip.ip, interface->mac.mac);

    /* add new interface to interface list */
    interface_list.insert(std::make_pair(interface_name, interface));
}


interface_t* get_interface_by_name(const char* interface_name)
{
    std::map<const char*,interface_t*>::iterator it;

    for (it = interface_list.begin(); it != interface_list.end(); ++it) {
        if (!strcmp(it->first, interface_name)) {
            return it->second;
        }
    }

    fprintf(stderr, "Error get_interface_by_name: interface %s not exist\n",
            interface_name);
    exit(1);
}


void get_interface_by_ip(ip_addr_t ip,
                         char* interface_name)
{
    std::map<const char*,interface_t*>::iterator it;

    for (it = interface_list.begin(); it != interface_list.end(); ++it) {
        int count = 0;
        for (int i = 0; i < IP_ADDR_SIZE; ++i) {
            if (it->second->ip.ip[i] == ip.ip[i]) ++count;
            else break;
        }
        if (count == IP_ADDR_SIZE) {
            strncpy(interface_name, it->first, MAX_INTERFACE_NAME_SIZE);
            return;
        }
    }

    interface_name = NULL;
    return;
}


void bind_to_ip(ip_addr_t ip)
{
    std::map<const char*,interface_t*>::iterator it;

    ip_addr_t default_ip = string_to_ip_addr(DEFAULT_IP);

    if (!compare_ip_addr(ip, default_ip, default_ip)) {
        for (it = interface_list.begin(); it != interface_list.end(); ++it) {
            interface_t* interface = it->second;
            int count = 0;
            for (int i = 0; i < IP_ADDR_SIZE; ++i) {
                if (interface->ip.ip[i] == ip.ip[i]) ++count;
                else break;
            }
            if (count == IP_ADDR_SIZE) return;
        }
        fprintf(stderr, "Error bind_to_ip: ip address does not exist\n");
        exit(1);
    }
}


