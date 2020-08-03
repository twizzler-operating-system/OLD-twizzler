#include "eth.h"

#include <stdio.h>

int main()
{
    //initialize queue and start consumer thread
    twzobj tx_queue_obj;
    twzobj rx_queue_obj;
    twz_object_init_name(&tx_queue_obj, "/dev/e1000-txqueue", FE_READ | FE_WRITE);
    twz_object_init_name(&rx_queue_obj, "/dev/e1000-rxqueue", FE_READ | FE_WRITE);
    //init_queue(&queue_obj);
    
    std::thread thr(recv, &rx_queue_obj);
   
    sleep(1);
    
    char test[] = "Sending this data.";
    send(test, &tx_queue_obj);
    
    fprintf(stderr, "main: SENT!\n");
    
    for(;;)
	    usleep(100000);
}
