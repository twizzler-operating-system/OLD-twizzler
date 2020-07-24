#include "eth.h"

void consumer(twzobj *qobj)
{
    printf("start consumer\n");

    while(1) {
        struct packet_queue_entry pqe;
        queue_receive(qobj, (struct queue_entry *)&pqe, 0);
        eth_hdr_t *eth_hdr = twz_object_lea(qobj, (eth_hdr_t *)pqe.ptr);
        (void)eth_hdr;
        std::cout<<"INCOMING MAC: "<< eth_hdr->dst_mac.mac <<std::endl;
        std::cout<<"INCOMING PAYLOAD: "<< eth_hdr->payload <<std::endl;

        queue_complete(qobj, (struct queue_entry *)&pqe, 0);
    }
}

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
static void *new_eth_frame_with_payload(twzobj *data_obj, char *data)
{
    eth_hdr_t *eth_hdr = (eth_hdr_t *)twz_object_base(data_obj);
     
    eth_hdr->dst_mac.mac[0] = 'b';
    eth_hdr->dst_mac.mac[1] = 'r';
    eth_hdr->dst_mac.mac[2] = 'o';
    eth_hdr->dst_mac.mac[3] = 'a';
    eth_hdr->dst_mac.mac[4] = 'd';
    eth_hdr->dst_mac.mac[5] = '\0';

    //eth_hdr->payload = (char *)malloc((strlen(test) + 1)*sizeof(test)); //learn how to do this in twizzler
    strcpy(eth_hdr->payload, data);

    std::cout << "Sending the payload: " << eth_hdr->payload << std::endl;
    
    //MUST FREE DATA AFTER SENDING IT!!
    
    return eth_hdr;
}


void init_queue(twzobj *qo)
{
    if(twz_object_new(qo, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE) < 0)
        abort();
        
    /* init the queue object, here we have 32 queue entries */
    queue_init_hdr(qo, 22, sizeof(struct packet_queue_entry), 8, sizeof(struct packet_queue_entry));
    
    /* start the consumer... */
    if(!fork()) consumer(qo);
    
}


void send_pkt(char *data, twzobj *queue_obj)
{
    twzobj data_obj;

    if(twz_object_new(&data_obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE) < 0)
        abort();

    struct packet_queue_entry pqe;
    
    /* get a unique ID (unique only for outstanding requests; it can be reused) */
    uint32_t info = get_new_info();

    /* store a pointer to some packet data */
    pqe.qe.info = info;
    pqe.ptr = twz_ptr_swizzle(queue_obj, new_eth_frame_with_payload(&data_obj, data), FE_READ);

    
    /* submit the packet! */
    queue_submit(queue_obj, (struct queue_entry *)&pqe, 0);

    release_info(pqe.qe.info);
}



int main()
{
    //initialize queue and start consumer thread
    twzobj queue_obj;
    init_queue(&queue_obj);
    
    char test[] = "Sending this data.";
    send_pkt(test, &queue_obj);
    
}
