import argparse
import os
import sys
import jinja2
import math
import numpy as np

def buildKs(n):
    if (n == 0):
        return []
    return buildKs(n//2) + [n] + buildKs(n//2)


# Try to reduce the number of size differences
# Only works for power of 2
def size_diffs(n):
    ks = buildKs(n//2);
    m=[[0] + [(j+1) * (-1)**( i//k ) for (j, k) in enumerate(ks)] for i in range(n)]

    for i, row in enumerate(m):
        m[i] = list(np.roll(row, i))
    m = np.apply_along_axis(lambda i: i%n, 1, m)
    return m

def lazy_diffs(n):
    arr = np.zeros((n, n), dtype=int)
    
    # Fill the diagonal with zeros
    np.fill_diagonal(arr, 0)
    
    # Rotate the numbers on each row
    for i in range(n):
        count = 1
        for j in range(n):
            if i != j:
                arr[i, j] = count
                count += 1
    
    return arr


# Calculate TCAM values for range
def tcam_values(min_val, max_val):
    dmax = max_val if min_val <= max_val else min_val
    #hex_width = ((int(math.log2(dmax)) + 4) // 4)
    hex_width = 8

    tcam_values = tcam_range(min_val, max_val)

    masks = []
    for val in tcam_values:
        masks.append(~(val[0] ^ val[1]) & ((1 << (hex_width * 4)) - 1))
    
    return list(zip([val[0] for val in tcam_values], masks))

def tcam_range(min_val, max_val):
    entry_tuples = []
    if min_val <= max_val:
        for i in range(64):
            mask = 1 << i
            if (min_val & mask) or (mask > max_val):
                break
        
        for j in range(i, -1, -1):
            stride = (1 << j) - 1
            if min_val + stride == max_val:
                entry_tuples.append((min_val,max_val))
                return entry_tuples
                
            elif min_val + stride < max_val:
                entry_tuples.extend(tcam_range(min_val, min_val + stride))
                entry_tuples.extend(tcam_range(min_val + stride + 1, max_val))
                return entry_tuples
    else:
        return tcam_range(max_val, min_val)



parser = argparse.ArgumentParser(
                prog='gen_compare_p4.py',
                description='Creates data plane and control logic for pivot detection')

parser.add_argument('registers', type=int, help="The args.registers of comparison registers to create")
parser.add_argument('-c', '--control', action='store_true', default=False, help="Generate the bfrt rules")
parser.add_argument('--test', action='store_true', default=False, help="Generate PTF tests")
parser.add_argument('-b', '--bytes_threshold', type=int, default=64, help="Size difference between pivot flows")
parser.add_argument('-t', '--time_threshold', type=int, default=100000000, help="Start time difference between pivot flows")
parser.add_argument('-to', '--timeout', type=int, default=300000000000, help="Compare registers timeout values")
parser.add_argument('--cache_size', type=int, default=65536, help="Size of cache register")
parser.add_argument('--compare_size', type=int, default=32768, help="Size of comparison registers")
parser.add_argument('-p4f', '--p4_file', type=str, default='spotlight.p4', help="Specify an output file for the p4 program")
parser.add_argument('-p4t', '--p4_template', type=str, default='spotlight.p4.template', help="Specify the location of the p4 template")
parser.add_argument('-bf', '--bfrt_file', type=str, default='spotlight_ctrl.py', help="Specify an output file for the bfrt rules")
parser.add_argument('-bt', '--bfrt_template', type=str, default='spotlight_ctrl.py.template', help="Specify the location of the bfrt template")

args = parser.parse_args()

if  not os.path.exists(args.p4_template):
    print(f"Compare p4 template not found at '{args.p4_template}'.")
    sys.exit(1)

if args.control:
    if  not os.path.exists(args.bfrt_template):
        print(f"Compare bfrt template not found at '{args.bfrt_template}'.")
        sys.exit(1)

if args.control:
    if  not os.path.exists(args.test_template):
        print(f"Test template not found at '{args.test_template}'.")
        sys.exit(1)
    


#Create dataplane
with open(args.p4_template,'r') as f:
    template_txt=f.read()
    t = jinja2.Template(template_txt,  trim_blocks=True, lstrip_blocks=True)

if(args.registers & (args.registers - 1) == 0 and args.registers != 0):
    size_fields = size_diffs(args.registers)
else:
    size_fields = lazy_diffs(args.registers)

print(size_fields)
output = (t.render(registers=args.registers,
                   cache_size=args.cache_size,
                   compare_size=args.compare_size,
                   size_fields=size_fields))
with open(args.p4_file, 'w') as f:
    f.write(output)

#Create control plane rules
if args.control:
    #Find mask values
    byte_ranges = [(0, args.bytes_threshold - 1),(2**32 - args.bytes_threshold, 2**32 - 1)]

    tcam_entrys = []
    for br in byte_ranges:
        tcam_entrys.extend(tcam_values(br[0], br[1]))
    
    with open(args.bfrt_template,'r') as f:
        template_txt=f.read()
        t = jinja2.Template(template_txt,  trim_blocks=True, lstrip_blocks=True)
    
    output = (t.render(registers=args.registers, tcam_entrys=tcam_entrys, prog_name=(args.p4_file[:-3]), index_timeout=args.timeout, compare_size=args.compare_size, time_threshold=args.time_threshold))
    
    with open(args.bfrt_file, 'w') as f:
        f.write(output)