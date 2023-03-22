import sys

# function to parse file and check if string 'LiveFlow' is present
def parse_file(file_name):
    total_fct = 0
    num_flows = 0
    total_size = 0
    with open(file_name, 'r') as f:
        for line in f:
            if 'LiveFlow' in line:
                split_line = line.split()
                fct = float(split_line[9])
                size = float(split_line[3])
                total_fct += fct
                total_size += size
                num_flows += 1

    avg_fct = total_fct / num_flows
    return avg_fct, num_flows, total_size

# parse file name as argument
if __name__ == '__main__':
    avg_fct_ms, num_flows, total_size = parse_file(sys.argv[1])
    avg_fct_ms = avg_fct_ms / 1000
    print("Average FCT: %.2f ms" % avg_fct_ms)
    print("Number of flows: %d" % num_flows)
    print("Total size: %.2f MB" % (total_size / 1000000))
