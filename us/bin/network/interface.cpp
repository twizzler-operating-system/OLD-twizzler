#include "interface.h"


static std::map<const char*,interface_t*> interface_list;


void init_interface(const char* interface_name,
                  const char* info_queue_name,
                  ip_addr_t interface_ip)
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
    /* initialize tx rx arp queue objs */
    char tx_queue_name[MAX_INTERFACE_NAME_SIZE];
    strcpy(tx_queue_name, interface->name);
    strcat(tx_queue_name, "-txqueue");
    if (twz_object_init_name(&interface->tx_queue_obj,
    tx_queue_name, FE_READ | FE_WRITE) < 0) {
        fprintf(stderr, "Error init_interface: could not init tx-queue\n");
        exit(1);
    }
    char rx_queue_name[MAX_INTERFACE_NAME_SIZE];
    strcpy(rx_queue_name, interface->name);
    strcat(rx_queue_name, "-rxqueue");
    if (twz_object_init_name(&interface->rx_queue_obj,
    rx_queue_name, FE_READ | FE_WRITE) < 0) {
        fprintf(stderr,"Error init_interface: could not init rx-queue\n");
        exit(1);
    }

    /* add new interface to interface list */
    interface_list.insert(std::make_pair(interface_name, interface));
}


interface_t* get_interface_by_name(const char* interface_name)
{
    std::map<const char*,interface_t*>::iterator it;

    it = interface_list.find(interface_name);
    if (it == interface_list.end()) {
        fprintf(stderr, "Error get_interface_by_name: interface %s not exist\n",
                interface_name);
        exit(1);
    }

    return it->second;
}
