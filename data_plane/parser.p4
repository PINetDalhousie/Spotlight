const bit<16> TYPE_IPV4 = 0x800;

const ip_protocol_t IP_PROTOCOLS_ICMP = 1;
const ip_protocol_t IP_PROTOCOLS_TCP = 6;
const ip_protocol_t IP_PROTOCOLS_UDP = 17;

/***********************  P A R S E R  **************************/
parser TofinoIngressParser(
        packet_in pkt,
        inout ingress_metadata_t ig_md,
        out ingress_intrinsic_metadata_t ig_intr_md) {
    state start {
        pkt.extract(ig_intr_md);
        transition select(ig_intr_md.resubmit_flag) {
            1 : parse_resubmit;
            0 : parse_port_metadata;
        }
    }

    state parse_resubmit {
        //pkt.extract(ig_md.flow_stats);
        transition accept;
    }

    state parse_port_metadata {
        pkt.advance(PORT_METADATA_SIZE);
        transition accept;
    }
}

parser IngressParser(packet_in        pkt,
    /* User */
    out ingress_headers_t          hdr,
    out ingress_metadata_t         meta,
    /* Intrinsic */
    out ingress_intrinsic_metadata_t  ig_intr_md)
{
    TofinoIngressParser() tofino_parser;

    state start {
        meta = {{0, 0, 0, 0, 0}, 0};
        tofino_parser.apply(pkt, meta, ig_intr_md);
        transition parse_ethernet;
    }

    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.ether_type) {
            TYPE_IPV4: parse_ipv4;
            default: accept;
        }
    }

    state parse_ipv4 {
		pkt.extract(hdr.ipv4);
		transition select(hdr.ipv4.protocol) {
			IP_PROTOCOLS_TCP : parse_l4_ports;
			IP_PROTOCOLS_UDP : parse_l4_ports;
			default : accept;
		}
	}

    state parse_l4_ports {
        pkt.extract(hdr.l4_ports);
        transition accept;
    }
}
