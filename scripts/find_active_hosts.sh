#!/bin/bash
if [ $# -ne 1 ]; then
	echo "Usage: $0 <pcap_filename>"
	exit 1
fi

pcap_file="$1"

tcpdump -r "$pcap_file" -n -q ip | awk -F" " '{split($3, src, "."); split($5, dst, "."); print src[1]"."src[2]"."src[3]"."src[4]; print dst[1]"."dst[2]"."dst[3]"."dst[4]}' | sort | uniq -c | sort -n -r 

