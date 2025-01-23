#include <iostream>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <pcap.h>
#include <chrono>
#include <cstring>
#include <arpa/inet.h>
#include <vector>
#include <netinet/ether.h>
#include <netinet/ip.h>    
#include <netinet/tcp.h>   
#include <netinet/udp.h>   
#include <atomic>
#include <cstdint>


//Parameters taken from APIVADS paper
#define PDF 4000000
#define PDS 1000000
#define PDT 10000
#define PN 10000

#define LPARAM 200
#define RPARAM 5
#define TWPARAM 30

std::atomic<int> global_id_counter{0}; 

int getNewID() {
    return global_id_counter++; 
}

struct Flow {
    std::string src_ip;
    uint16_t src_port;
    std::string dst_ip;
    uint16_t dst_port;
    uint8_t protocol;

    Flow() : src_ip(""), src_port(0), dst_ip(""), dst_port(0), protocol(0) {}
    Flow(const std::string& src_ip, uint16_t src_port, const std::string& dst_ip, uint16_t dst_port, uint8_t protocol)
        : src_ip(src_ip), src_port(src_port), dst_ip(dst_ip), dst_port(dst_port), protocol(protocol) {}
};

struct FlowReport {
    int flow_id;
    Flow flow;
    int packet_count;
    uint64_t byte_count;
    std::vector<uint64_t> timestamps; // List of timestamps for packets

    FlowReport() : packet_count(0), byte_count(0), duration(0) {flow_id=getNewID();}

    FlowReport(const Flow& f) : flow(f), packet_count(1), byte_count(0), duration(0) {flow_id=getNewID();}
};

struct BiFlowPair {
    FlowReport flow1;
    FlowReport flow2;

    BiFlowPair(const FlowReport& f1, const FlowReport& f2) : flow1(f1), flow2(f2) {}
};

std::mutex queue_mutex;
std::unordered_map<std::string, FlowReport> flow_map;

std::string create_flow_key(const Flow& flow) {
    // Sort IPs and ports to create a unique key for bi-directional flows
    if (flow.src_ip < flow.dst_ip || (flow.src_ip == flow.dst_ip && flow.src_port < flow.dst_port)) {
        return flow.src_ip + ":" + std::to_string(flow.src_port) + "->" +
               flow.dst_ip + ":" + std::to_string(flow.dst_port) + ":" +
               std::to_string(flow.protocol);
    } else {
        return flow.dst_ip + ":" + std::to_string(flow.dst_port) + "->" +
               flow.src_ip + ":" + std::to_string(flow.src_port) + ":" +
               std::to_string(flow.protocol);
    }
}

void packet_handler(u_char* args, const struct pcap_pkthdr* header, const u_char* packet) {
    const struct ip* ip_header = (struct ip*)(packet + sizeof(struct ether_header));
    uint16_t src_port, dst_port;
    uint8_t protocol = ip_header->ip_p;

    if (protocol == IPPROTO_TCP) {
        const struct tcphdr* tcp_header = (struct tcphdr*)(packet + sizeof(struct ether_header) + sizeof(struct ip));
        src_port = ntohs(tcp_header->th_sport);
        dst_port = ntohs(tcp_header->th_dport);
    } else if (protocol == IPPROTO_UDP) {
        const struct udphdr* udp_header = (struct udphdr*)(packet + sizeof(struct ether_header) + sizeof(struct ip));
        src_port = ntohs(udp_header->uh_sport);
        dst_port = ntohs(udp_header->uh_dport);
    } else {
        return; // Not TCP or UDP
    }

    Flow flow(inet_ntoa(ip_header->ip_src), src_port, inet_ntoa(ip_header->ip_dst), dst_port, protocol);

    // Create a unique key for the flow
    std::string flow_key = create_flow_key(flow);

    // Get the current timestamp
    auto now = std::chrono::system_clock::now();
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();

    // Lock and update the flow map
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (flow_map.find(flow_key) == flow_map.end()) {
            flow_map[flow_key] = FlowReport(flow);
        }
        // Update packet count, byte count, and add timestamp
        flow_map[flow_key].packet_count++;
        flow_map[flow_key].byte_count += header->len; // Add packet length to byte count
        flow_map[flow_key].timestamps.push_back(now_us); // Store the timestamp
    }
}

int computeTimestampDiff(const std::vector<uint64_t>& timestamps) {
    if (timestamps.size() < 2) {
	return 0;
    }

    return timestamps.back() - timestamps.front();
}


std::vector<BiFlowPair> durationFilter(std::unordered_map<std::string, FlowReport>& flow_map) {
    std::vector<BiFlowPair> biflow_pairs;

    //Compute durations and remove ephemeral flows
    for(auto it = flow_map.begin(); it != flow_map.end();) {
	it->second.duration = computeTimestampDiff(it->second.timestamps);
	if (it->second.duration < PDF) {
	    it = flow_map.erase(it);
	}
	else {
	    it++;
	}
    }

    for(auto it1 = flow_map.begin(); it1 != flow_map.end(); ++it1) {
    	for(auto it2 = std::next(it1); it2 != flow_map.end(); ++it2) {
	    //filter duration
	    auto duration_diff = (it1->second.duration > it2->second.duration) ? (it1->second.duration - it2->second.duration) :
		                                                                 (it2->second.duration - it1->second.duration);
	    if (duration_diff < PDT){
		//filter start time
		auto start_time_diff = (it1->second.timestamps[0] > it2->second.timestamps[0]) ? (it1->second.timestamps[0] - it2->second.timestamps[0]) :
			                                                                         (it2->second.timestamps[0] - it1->second.timestamps[0]);
		if (start_time_diff < PDS) {
		    //filter size ratio
		    //double ratio = it1->second.byte_count / it2->second.byte_count;
		    //if (ratio > PN) { used in BitTorrent only
		    	biflow_pairs.push_back(BiFlowPair(it1->second, it2->second));
		    //}
		}
	    }
	}
    }

    return biflow_pairs;
}


std::vector<int> merge(const BiFlowPair& test_pair) {
    std::vector<int> merge;

    auto& flow1 = test_pair.flow1;
    auto& flow2 = test_pair.flow2;


    int i = flow1.timestamps.size() - 1;
    int j = flow2.timestamps.size() - 1;
    int l_count = 0;

    
    while(l_count < LPARAM && i >= 0 && j >= 0) {
        if (flow1.timestamps[i] < flow2.timestamps[j]) {
	    //Push flow2
	    merge.push_back(flow2.flow_id);
	    j--;
	    }
        else {
        //Push flow1
        merge.push_back(flow1.flow_id);
            i--;
        }

        l_count++;
    }

    while(l_count < LPARAM && i >= 0) {
        merge.push_back(flow1.flow_id);
        i--;
    }

    while(l_count < LPARAM && j >= 0) {
        merge.push_back(flow2.flow_id);
        j--;
    }

    return merge;
}

//Check how many packets are in a row before a change
int alternation_count(const std::vector<int>& merged_list) {
    int alt_count = 1;
    int max_alt = 1;
    
    for(int i = 1; i < merged_list.size(); i++) {
	if (merged_list[i] == merged_list[i-1]) {
	    alt_count++;
	}
	else {
	    if (alt_count > max_alt) {
	        max_alt = alt_count;
	    }
	    alt_count = 1;
	}
    }

    if (alt_count > max_alt) {
	max_alt = alt_count;
    }

    return max_alt;
}

//Filter out pairs with many of the same flows packets in a row
void alternationFilter(std::vector<BiFlowPair>& suspect_pairs) {
    for(auto it = suspect_pairs.begin(); it != suspect_pairs.end();) {
        auto merged_list = merge(*it);

        auto alt_count = alternation_count(merged_list);

        if (alt_count > RPARAM) {
            it = suspect_pairs.erase(it);
        }
        else {
            ++it;
        }
    }
}

bool checkQuarter(const std::vector<int>& vec, int value1, int value2, int start, int end) {
    bool foundValue1 = false;
    bool foundValue2 = false;

    for (int i = start; i < end; ++i) {
        if (vec[i] == value1) {
            foundValue1 = true;
        }
        if (vec[i] == value2) {
            foundValue2 = true;
        }

        if (foundValue1 && foundValue2) {
            return true;
        }
    }
    return false;
}

void coherenceFilter(std::vector<BiFlowPair>& suspect_pairs) {
     for(auto it = suspect_pairs.begin(); it != suspect_pairs.end();) {
	auto merged_list = merge(*it);
	int quarter = merged_list.size()/4;

    //Check each quarter of the list for the two flows
	if(checkQuarter(merged_list, it->flow1.flow_id, it->flow2.flow_id, 0, quarter) &&
	   checkQuarter(merged_list, it->flow1.flow_id, it->flow2.flow_id, quarter, quarter * 2) &&
	   checkQuarter(merged_list, it->flow1.flow_id, it->flow2.flow_id, quarter * 2, quarter * 3) &&
           checkQuarter(merged_list, it->flow1.flow_id, it->flow2.flow_id, quarter * 3, merged_list.size())) {
	    ++it;
	}
	else {
	    it = suspect_pairs.erase(it);
	}
     }
}

void report_printer() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(TWPARAM));

        std::unordered_map<std::string, FlowReport> local_flow_map;
        //Retrieve the flow map and clear it
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            local_flow_map = std::move(flow_map);
            flow_map.clear(); // Clear the flow map for the next interval
        }

    //Filter the flows and report whats left
	auto biflow_pairs = durationFilter(local_flow_map);
	alternationFilter(biflow_pairs);
	coherenceFilter(biflow_pairs);

	for (const auto& pair : biflow_pairs) {
	   std::cout  << "PIVOT!: " << pair.flow1.flow.src_ip << ":" << pair.flow1.flow.src_port << " <-> " 
		                    << pair.flow1.flow.dst_ip << ":" << pair.flow1.flow.dst_port << " | "
		                    << pair.flow2.flow.src_ip << ":" << pair.flow2.flow.src_port << " <-> "
				    << pair.flow2.flow.dst_ip << ":" << pair.flow2.flow.dst_port << std::endl;
	}
    }
}

void capture_packets(const char* interface) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t* handle = pcap_open_live(interface, BUFSIZ, 1, 1000, errbuf);
    if (handle == nullptr) {
        std::cerr << "Could not open device: " << errbuf << std::endl;
        return;
    }
    pcap_loop(handle, 0, packet_handler, nullptr);
    pcap_close(handle);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <interface>" << std::endl;
        return 1;
    }

    std::string interface = argv[1];

    std::thread capture_thread(capture_packets, interface.c_str());
    std::thread print_thread(report_printer);

    capture_thread.join();
    print_thread.join();

    return 0;
}
