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
    
    memset((void*)eth_hdr->dst_mac.mac, 'b', MAC_ADDR_SIZE*(sizeof(char))); //using broacast address for now
    //must set the source mac (interface with NIC)
    //also memset all all other fields to zero

    //char *payload = (char *) eth_hdr + SIZE_OF_ETH_HDR_EXCLUDING_PAYLOAD;
    //eth_hdr->payload = (char *)malloc((strlen(data) + 1)*sizeof(data)); //learn how to do this in twizzler and malloc more data in the data object
    strcpy(eth_hdr->payload, data);

    std::cout << "Sending the payload: " << eth_hdr->payload << std::endl;
    
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


//must set a max buffer size, once new_eth_frame_with_payload funcation gets correctly modified to deal with malloc
void send(char *data, twzobj *queue_obj) //needs to be modified to also include destination, & also deal if destination is NULL
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

void recv(twzobj *queue_obj, char *recv_buffer)
{
    struct packet_queue_entry pqe;
    queue_receive(queue_obj, (struct queue_entry *)&pqe, 0);
    eth_hdr_t *eth_hdr = twz_object_lea(queue_obj, (eth_hdr_t *)pqe.ptr);
    (void)eth_hdr;
    
    std::cout<<"INCOMING MAC: "<< eth_hdr->dst_mac.mac <<std::endl;

    strcpy(recv_buffer, eth_hdr->payload);
    queue_complete(queue_obj, (struct queue_entry *)&pqe, 0);
}
