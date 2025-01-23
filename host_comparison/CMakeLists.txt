cmake_minimum_required(VERSION 3.10)

project(host_comp)


set(PCAP_INCLUDE_DIRS "/usr/include")  # Adjust based on your system
set(PCAP_LIBRARIES "/usr/lib/x86_64-linux-gnu/libpcap.so")  # Adjust based on your system
include_directories(${PCAP_INCLUDE_DIRS})

set(SOURCES host_comp.cpp)

# Create the executable target
add_executable(host_comp ${SOURCES})

# Link the PCAP library to the target
target_link_libraries(host_comp ${PCAP_LIBRARIES})

