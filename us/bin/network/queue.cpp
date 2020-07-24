#include "eth.h"

int main()
{
    //initialize queue and start consumer thread
    twzobj queue_obj;
    init_queue(&queue_obj);
    
    char test[] = "Sending this data.";
    send(test, &queue_obj);
    
    
    char recv_buffer [248];
    recv(&queue_obj, recv_buffer);
    std::cout<<"Got data "<<recv_buffer<<std::endl;
    
}
