import subprocess
import random
import time
import sys

SLEEP_TIME = 5

def replace_ip_and_replay(attacks, internal, attackFile, interface):
    with open(internal, 'r') as f1:
        ip_list = [line.strip() for line in f1.readlines()]

    for i in range(attacks):
        ip_to_use = ip_list[i]
        command = [
            "tcpreplay-edit",
            f"intf1={interface}", 
            f"--pnat=10.0.0.2:{ip_to_use}",
            "--mtu-trunc",
            attackFile
        ]
        
        try:
            print(f"Running tcp-replay with IP: {ip_to_use}")
            subprocess.run(command, check=True)
        except subprocess.CalledProcessError as e:
            print(f"Error occurred while running tcp-replay: {e}")
            continue
        
        sleep_time = random.uniform(1, SLEEP_TIME)
        time.sleep(sleep_time)

# Example usage:
if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("Usage: python script.py <number of attacks> <internal hosts file> <attack pcap> <interface>")
        sys.exit(1)
    
    attacks = int(sys.argv[1])
    internal = sys.argv[2]
    attackFile = sys.argv[3]
    interface = sys.argv[4]

    replace_ip_and_replay(attacks, internal, attackFile, interface)

