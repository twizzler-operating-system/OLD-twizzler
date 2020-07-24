#include "eth.cpp"

/*ORP APIs*/
void init_orp_map(twzobj *orp_obj)
{
    
}
void add_orp_entry(twzobj *orp_table)
{
    
}
void remove_orp_entry(twzobj *orp_table, obj_id_t id) //when should we delete, how do we know entry is expired and obj_id has moved?
{
    
}
mac_addr_t orp_lookup(twzobj *orp_table, obj_id_t id)
{
    
}



/*ORP protocol implementation*/


void send_orp_request(obj_id_t id)
{
    
}

void send_orp_reply(eth_hdr_t *eth_hdr)
{
    
}

void recv_prp_request(eth_hdr_t *eth_hdr)
{
    
}

void recv_orp_reply(eth_hdr_t *eth_hdr)
{
    
}
