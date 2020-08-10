#include <stdio.h>

#include "intr_props.h"

//argv[1] should be IP address
int main(int argc, char *argv[])
{
    //initialize interface and create object to store inferface info under /dev/e1000
    //FUTURE WORK: having multiple NICs
    twzobj interface_obj;
    init_interface("/dev/e1000", "/dev/e1000-info", &interface_obj);

    //assign ipv4 settings of interface
    if(argc > 1)
    {
        ipv4_addr_t ipv4;
        strncpy(ipv4.addr, argv[1], MAX_IPV4_CHAR_SIZE);

        if(!assign_ipv4_addr_to_intr(&ipv4, &interface_obj))
            fprintf(stderr,"@main: Could not assign IP addr to interface. Operating in L2 mode");
            //we must eventually be able to self assign IP address
    }


    //initialize rx and tx queue objs, and start reciever thread
    twzobj tx_queue_obj;
    twzobj rx_queue_obj;
    if(twz_object_init_name(&tx_queue_obj, "/dev/e1000-txqueue", FE_READ | FE_WRITE) < 0)
    {
        fprintf(stderr,"@main: Could not init e1000-txqueue");
        exit(1);
    }
    if(twz_object_init_name(&rx_queue_obj, "/dev/e1000-rxqueue", FE_READ | FE_WRITE) < 0)
    {
        fprintf(stderr,"@main: Could not init e1000-rxqueue");
        exit(1);
    }
    
    //start reciever thread
    std::thread thr(recv, &rx_queue_obj);
   
    sleep(1);
    
    
    
    char test[] = "Sending this data.";
    send(test, &tx_queue_obj, &interface_obj);
    
    fprintf(stderr, "@main: SENT!\n");
    
    for(;;)
        usleep(100000);
}
