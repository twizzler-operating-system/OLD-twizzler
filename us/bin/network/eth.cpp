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

/* some example code for making a packet. We have a data object, and we're choosing pages to fill
 * with data */
static void *new_eth_frame_with_payload(twzobj *interface_obj, twzobj *data_obj, char *data)
{
    eth_hdr_t *eth_hdr = (eth_hdr_t *)twz_object_base(data_obj);
    interface_t *interface = (interface_t *)twz_object_base(interface_obj);

    memcpy(eth_hdr->src_mac.mac, interface->mac.mac, MAC_ADDR_SIZE);
    
    
    //setting destination MAC (eventually must be taken in as an argument, vs using broadcast address
    memset((void*)eth_hdr->dst_mac.mac, 0xff , MAC_ADDR_SIZE*(sizeof(char)));
//
//    //for testing purposes
//    eth_hdr->dst_mac.mac[0] = 0x86;
//    eth_hdr->dst_mac.mac[1] = 0x1a;
//    eth_hdr->dst_mac.mac[2] = 0x6c;
//    eth_hdr->dst_mac.mac[3] = 0x30;
//    eth_hdr->dst_mac.mac[4] = 0x34;
//    eth_hdr->dst_mac.mac[5] = 0x28;
    
    

   // eth_hdr->type = 0x2020;

    
    //also memset all all other fields to zero

    //char *payload = (char *) eth_hdr + SIZE_OF_ETH_HDR_EXCLUDING_PAYLOAD;
    //eth_hdr->payload = (char *)malloc((strlen(daita) + 1)*sizeof(data)); //learn how to do this in twizzler and malloc more data in the data object
    strcpy(eth_hdr->payload, interface->ipv4_addr.addr);

    
    //MUST FREE DATA AFTER SENDING IT (send function)!!
    
    return eth_hdr;
}


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


//must set a max buffer size, once new_eth_frame_with_payload function gets correctly modified to deal with malloc
void l2_send(char *data, twzobj *queue_obj, twzobj *interface_obj) //needs to be modified to also include destination, & also deal if destination is NULL
{
    twzobj data_obj;

    /*cannot create new object every time.. this must be changed!!!*/
    if(twz_object_new(&data_obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE) < 0)
        abort();

    struct packet_queue_entry pqe;
    
    /* get a unique ID (unique only for outstanding requests; it can be reused) */
    uint32_t info = get_new_info();

    /* store a pointer to some packet data */
    pqe.qe.info = info;
    pqe.ptr = twz_ptr_swizzle(queue_obj, new_eth_frame_with_payload(interface_obj, &data_obj, data), FE_READ);
    pqe.len = strlen(data) + 14;

    
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
        
        
        eth_hdr_t *eth_hdr = (eth_hdr_t *)(ph + 1);
        (void)eth_hdr;
        
        //std::cout<<"INCOMING MAC: "<< eth_hdr->dst_mac.mac <<std::endl;

        std::cout<<"Incoming data: "<<eth_hdr->payload<<std::endl;
        queue_complete(queue_obj, (struct queue_entry *)&pqe, 0);
    }
}
