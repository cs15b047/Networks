import sys
import os

# function to parse file and check if string 'LiveFlow' is present
def parse_file(file_name):
    total_fct = 0
    num_flows = 0
    total_size = 0
    utilization = 0
    with open(file_name, 'r') as f:
        for line in f:
            if 'utilization' in line:
                split_line = line.split(" : ")
                utilization = float(split_line[1])
            if 'LiveFlow' in line:
                split_line = line.split()
                fct = float(split_line[9])
                size = float(split_line[3])
                total_fct += fct
                total_size += size
                num_flows += 1

    avg_fct = total_fct / num_flows
    return avg_fct, num_flows, total_size, utilization

# parse file name as argument
if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python parse_results.py input_file output_dir")
        sys.exit(1)

    input_file = sys.argv[1]
    output_dir = sys.argv[2]

    # extract parent directory name from input file name
    parent_dir_name = input_file.split("/")[-2]
    output_dir = output_dir + "/" + parent_dir_name
    # create output directory if it does not exist
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    output_file = "-".join(input_file.split("/")[-1].split(".")[0].split("-")[:-1]) + ".csv"
    output_file = output_dir + "/" + output_file

    avg_fct_ms, num_flows, total_size, utilization = parse_file(input_file)
    avg_fct_ms = avg_fct_ms / 1000
    total_size_mb = total_size / 1000000

    # write avg_fct_ms and utilization to output file as csv
    with open(output_file, 'a') as f:
        f.write("%s,%.2f,%s,%.2f\n" % (utilization, avg_fct_ms, num_flows, total_size_mb))

    print("Wrote results to %s" % output_file)


