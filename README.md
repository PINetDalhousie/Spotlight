# Spotlight

## Table of Contents
1. [Overview](#overview)
2. [Installation](#installation)
3. [Usage](#usage)
4. [Experiments] (#Experiments)
5. [License](#license)

## Overview
Software artifacts for Spotlight: Shining a Light on Pivot Attacks Using In-network Computing. Repo contains data plane code, control plane rules, attack traces, and our host comparison version. Our data and controle plane is written using [Jinja](https://jinja.palletsprojects.com/en/stable/) to allow for parameter variation. Once code is generated from the templates, it can be run on a [Tofino switch](https://github.com/barefootnetworks/Open-Tofino).

We also provide our attack traces and some support scripts to help reproduce our results.

## Installation
To install the dependancies to generate the Tofino code:

```bash
# Clone the repository
git clone https://github.com/PINetDalhousie/Spotlight.git

# Navigate to the project directory
cd Spotlight

# Install dependencies
pip install -r requirements.txt
```

The data plane was tested using bf-sde-9.7.0. After generating the p4 code and correponding rules compile and run on Tofino switch.

To run the code for the host comparison:
```bash
cd host_comparison

mkdir build
cd build

cmake ..
make
```


## Usage
To generate p4 and control plane rules, provide the file names as well as parameter values. For example, generating Spotlight with 8 registers and a default time window and size difference is done using the following command:

```bash
python scripts/gen_spotlight.py 8 -c -p4t data_plane/spotlight.p4.template -p4f data_plane/spotlight.p4 -bt control_plane/spotlight_ctrl.py.template -bf control_plane/spotlight_ctrl.py
```

Note: You will need to update the report port to make the physical connections on your switch. Packets idenfified as pivoting with be forwarded to this port. You will also need to modify the control plane rules to point to your hosts file generated below.

To reproduce our results attacks are provided in the attacks folder. Background traffic is from the Monday in [CICIDS2017](https://www.unb.ca/cic/datasets/ids-2017.html) and from March 1, 2023 to March 21, 2023
[MAWI traces](https://mawi.wide.ad.jp/mawi/samplepoint-F/2023/).

The control plane rules populate tables with IP addresses from a file. For our experiments we used the most active hosts within a particular trace. Move this file to the switch when setting up.

```bash
./scripts/find_active_hosts.sh PCAP_FILE | head -n NUM_ACTIVE_HOSTS | awk '{print $2}' > internal.hosts
```

Replay the background traffic and the attack you wish to test. To "attack" a node in the background trace:

```bash
tcpreplay-edit --intf1=INTERFACE --pnat=10.0.0.2:NODE_IP ATTACK_FILE
```

## Experiments
Here, you’ll find detailed, step-by-step instructions to help recreate our experiments using the code provided in this repository. The instructions here use freely available [Open P4Studio](https://github.com/p4lang/open-p4studio) and Tofino model, which should be installed prior to running the experiments. Our experiments used a physical switch, so there may be some deviation due to performance contraints. 

Begin by installing P4Studio and setting the environment variables using scripts in that repository, SDE and SDE_INSTALL.
```bash
~/open-p4studio/create-setup-script.sh > ~/setup-open-p4studio.bash
source ~/setup-open-p4studio.bash
```

If it is your first time running the model, create the interfaces for the model.
```bash
sudo ${SDE_INSTALL}/bin/veth_setup.sh 128
```

Generate the data plane and control plane code.
```bash
python scripts/gen_spotlight.py 8 -c -p4t data_plane/spotlight.p4.template -p4f data_plane/spotlight.p4 -bt control_plane/spotlight_ctrl.py.template -bf control_plane/spotlight_ctrl.py
```

Compile the P4 code using the P4 compiler. Copy the artifacts to the SDE.
```bash
$SDE_INSTALL/bin/p4c data_plane/spotlight.p4
cp -r spotlight.tofino $SDE_INSTALL
cp spotlight.tofino/spotlight.conf $SDE_INSTALL/share/p4/targets/tofino/
```

Prepare the dataset for the experiment. The example uses the CICIDS2017 data with 1000 internal nodes.
```bash
./scripts/find_active_hosts.sh Monday-WorkingHours.pcap | head -n 1000 | awk '{print $2}' > internal.hosts
```

To run the P4 program we need three terminals, one for the model, switch data plane, and control rules. Don't forget to set the environment variables in each window.

Terminal 1. Run the tofino model.
```bash
$SDE/run_tofino_model.sh -p spotlight
```

Terminal 2. Run the data plane program.
```bash
$SDE/run_switchd.sh -p spotlight
```

Terminal 3. Add the control plane rules.
```bash
$SDE/run_bfshell.sh -b control_plane/spotlight.py -i
```

Now that the switch is up an running, open two final terminals to run the experiment. One to collect report traffic and one to replay the pcap files.

Run a sniffer on the report port.
```bash
sudo tcpdump -i veth4 -w report.pcap
```

Start the background traffic.
```bash
sudo tcpreplay -i veth0 Monday-WorkingHours.pcap &
```

Run the attacks. A Python script will run pivot attacks on the internal nodes. Here we run attacks on the 50 most active nodes.
```bash
python scripts/run_attacks.py 50 internal.hosts pivot_attacks/chisel.pcap veth0
```

The CICIDS2017 pcap is 8hrs long. If you want a complete report, i.e., with all false positives, let it run for the entire duration. To just check detected attacks end the report after the attack script finishes.

Analyse the report by check for reported packets with the target IP. We use tshark to find conversations. There should be an entry for every attack.
```bash
tcpdump -r report.pcap -w - host 10.0.0.3 | tshark -r - -q -z conv,tcp
```

## License
MIT License

Copyright (c) 2025 Carson Kuzniar

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
