#include "eth.h"

#include <stdio.h>

int main()
{
    //initialize queue and start consumer thread
    twzobj queue_obj;
    twz_object_init_name(&queue_obj, "/dev/e1000-txqueue", FE_READ | FE_WRITE);
    //init_queue(&queue_obj);
    
    fprintf(stderr, "main: sending\n");
    char test[] = "Sending this data.";
    send(test, &queue_obj);
    
    fprintf(stderr, "main: SENT!\n");
    
    return 0;
    twzobj rx_queue_obj;
    twz_object_init_name(&rx_queue_obj, "/dev/e1000-rxqueue", FE_READ | FE_WRITE);
    char recv_buffer [248];
    recv(&rx_queue_obj, recv_buffer);
    std::cout<<"Got data "<<recv_buffer<<std::endl;
    
}
