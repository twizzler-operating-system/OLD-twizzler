#include "flip.h"

/* this code is for ensuring that every packet we send has a unique ID, so that when we get back the
 * completion we know which packet was sent */
static uint32_t counter = 0;
static std::vector<uint32_t> info_list;
static uint32_t get_new_info()
{
    if(info_list.size() == 0) {
        return ++counter;
    }
    uint32_t info = info_list.back();
    info_list.pop_back();
    return info;
}

static void release_info(uint32_t info)
{
    info_list.push_back(info);
}


static void *new_eth_frame_with_payload(mac_addr_t *dest_mac, twzobj *interface_obj, void *eth_ptr, uint16_t type)
{
    eth_hdr_t *eth_hdr = (eth_hdr_t *)eth_ptr;
    interface_t *interface = (interface_t *)twz_object_base(interface_obj);

    /*Source MAC*/
    memcpy(eth_hdr->src_mac.mac, interface->mac.mac, MAC_ADDR_SIZE);

    /*Destination MAC*/
    memcpy(eth_hdr->dst_mac.mac, dest_mac->mac, MAC_ADDR_SIZE);

    /*Payload Type*/
    eth_hdr->type = type;

    return eth_hdr;
}



void l2_send(mac_addr_t *dest_mac, twzobj *queue_obj, twzobj *interface_obj, void *pkt_ptr, uint16_t type, int len)
{
    struct packet_queue_entry pqe;
    
    /* get a unique ID (unique only for outstanding requests; it can be reused) */
    uint32_t info = get_new_info();

    /* store a pointer to some packet data */
    pqe.qe.info = info;
    pqe.ptr = twz_ptr_swizzle(queue_obj, new_eth_frame_with_payload(dest_mac, interface_obj, pkt_ptr, type), FE_READ);
    pqe.len = len;

    
    /* submit the packet! */
    queue_submit(queue_obj, (struct queue_entry *)&pqe, 0);

    //release_info(pqe.qe.info);
}



void l2_recv(twzobj *queue_obj)
{
    while(1)
    {
        struct packet_queue_entry pqe;
        queue_receive(queue_obj, (struct queue_entry *)&pqe, 0);
        fprintf(stderr, "Reciever got packet!\n");

        /* packet structure from the nic starts with a packet_header struct that contains some
         * useful information (or, will in the future), followed by the actual packet data. */
        struct packet_header *ph = (struct packet_header *)twz_object_lea(queue_obj, pqe.ptr);


        eth_hdr_t *pkt_ptr = (eth_hdr_t *)(ph + 1);

        /*Decapsulate then send to higher layers*/
        //if its plain FLIP type
        if(pkt_ptr->type == FLIP_TYPE)
            fprintf(stderr, "Got FLIP PACKET!\n");

        //if its ARP type


        queue_complete(queue_obj, (struct queue_entry *)&pqe, 0);
    }
}








//not sure if we will need this function, not using it at all
void init_queue(twzobj *qo)
{
    if(twz_object_new(qo, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE) < 0)
        abort();

    /* init the queue object, here we have 32 queue entries */
    queue_init_hdr(qo, 22, sizeof(struct packet_queue_entry), 8, sizeof(struct packet_queue_entry));

    /* start the consumer... */
    //if(!fork()) consumer(qo);
    //this will need to be a thread running on the background, but for now, we'll manually read from buffer

}
