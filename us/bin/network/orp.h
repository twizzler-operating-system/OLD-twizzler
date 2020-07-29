//forward decleration
typedef struct mac_addr mac_addr_t;
typedef struct obj_id obj_id_t;
typedef struct eth_hdr eth_hdr_t;

#pragma pack(push,1)
typedef struct orp_hdr
{
    short hw_type; // 1 for ethernet
    short proto_type; //type for FLIP
    char hw_addr_len; // value: 6 for MAC
    char proto_addr_len; //value: 128 for obj_id
    short opcode; //req or reply
    mac_addr_t *src_mac;
    mac_addr_t *dst_mac;
    obj_id_t *dst_obj_id;
}orp_hdr_t;
#pragma pack(pop)

typedef struct orp_table
{
    std::map<obj_id_t, mac_addr_t> orp_lookup_table;
}orp_table_t;



/*ObjectResolutionProtocol APIs*/
void init_orp_map(orp_table_t *orp_table);
void add_orp_entry(orp_table_t *orp_table);
void remove_orp_entry(orp_table_t *orp_table, obj_id_t id); //when should we delete, how do we know entry is expired and obj_id has moved?
mac_addr_t orp_lookup(orp_table_t *orp_table, obj_id_t id);
//add API to display ORP Table

/*ORP Protocol Implementation*/
void send_orp_request(obj_id_t id);
void send_orp_reply(eth_hdr_t *eth_hdr);
void recv_prp_request(eth_hdr_t *eth_hdr);
void recv_orp_reply(eth_hdr_t *eth_hdr);
