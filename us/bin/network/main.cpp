#include <stdio.h>

#include "flip.h"

//argv[1] should be IP address
int main(int argc, char *argv[])
{
    
    //initialize interface and create object to store inferface info under /dev/e1000
    twzobj interface_obj;
    init_interface("/dev/e1000", "/dev/e1000-info", &interface_obj);

    //assign ipv4 settings of interface
    ipv4_addr_t ipv4;
    if(argc > 1)
    {
        strncpy(ipv4.addr, argv[1], MAX_IPV4_CHAR_SIZE);

        if(!assign_ipv4_addr_to_intr(&ipv4, &interface_obj))
            fprintf(stderr,"@main: Could not assign IP addr to interface. Operating in L2 mode.\n");
    }
    else
    {
        fprintf(stderr, "Please provide IP address as argument.\n");
        exit(1);
    }



    //initialize rx and tx queue objs, and start reciever thread
    twzobj tx_queue_obj;
    twzobj rx_queue_obj;
    if(twz_object_init_name(&tx_queue_obj, "/dev/e1000-txqueue", FE_READ | FE_WRITE) < 0)
    {
        fprintf(stderr,"@main: Could not init e1000-txqueue.\n");
        exit(1);
    }
    if(twz_object_init_name(&rx_queue_obj, "/dev/e1000-rxqueue", FE_READ | FE_WRITE) < 0)
    {
        fprintf(stderr,"@main: Could not init e1000-rxqueue.\n");
        exit(1);
    }
    
    //start reciever thread
    std::thread thr(l2_recv, &rx_queue_obj, &interface_obj);
    

    
    char test_data[] = "TEST DATA.";
    uint8_t meta1 = 0b10110000;
    uint8_t meta2 = 0b01000000;
    char destination[MAX_IPV4_CHAR_SIZE] = "2.2.2.2";
    
    flip_send(meta1, meta2, NULL, 0x2020, destination, test_data, &interface_obj, &tx_queue_obj);
    
    fprintf(stderr, "@main: SENT!\n");
    
    for(;;)
        usleep(100000);
}
