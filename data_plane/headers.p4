/*************************************************************************
 ***********************  H E A D E R S  *********************************
 *************************************************************************/

typedef bit<8> ip_protocol_t;
typedef bit<32> ipv4_addr_t;
typedef bit<16> cache_index_t;

header ethernet_h {
    bit<48>   dst_addr;
    bit<48>   src_addr;
    bit<16>   ether_type;
}

header ipv4_h {
	bit<4> version;
	bit<4> ihl;
	bit<8> diffserv;
	bit<16> total_len;
	bit<16> identification;
	bit<3> flags;
	bit<13> frag_offset;
	bit<8> ttl;
	bit<8> protocol;
	bit<16> hdr_checksum;
	ipv4_addr_t src_addr;
	ipv4_addr_t dst_addr;
}

header l4_ports_h {
	bit<16> src_port;
	bit<16> dst_port;
}

struct ingress_headers_t {
    ethernet_h   ethernet;
    ipv4_h       ipv4;
    l4_ports_h   l4_ports;
}


/******  G L O B A L   I N G R E S S   M E T A D A T A  *********/

struct flow_stats_t {
    bit<32> flow_id;
    cache_index_t flow_idx;
    bit<32> start_time;
    bit<32> evict_flow_id;
    bit<32> evict_time;
}

struct ingress_metadata_t {
    flow_stats_t flow_stats;
    //MirrorId_t mirror_session;
    bit<1> map_hit;
}

//Header for digests
header flow_report_h {
    bit<32> flow_id;
	cache_index_t flow_idx;
	bit<32> start_time;
	ipv4_addr_t src_addr;
	ipv4_addr_t dst_addr;
	bit<32> evict_flow_id;
    bit<32> evict_time;
}

