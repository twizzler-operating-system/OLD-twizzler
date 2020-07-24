#include "eth.h"

int main()
{
    //initialize queue and start consumer thread
    twzobj queue_obj;
    init_queue(&queue_obj);
    
    char test[] = "Sending this data.";
    send_pkt(test, &queue_obj);
    
    
}
