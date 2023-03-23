import sys
import os

# function to parse file and check if string 'LiveFlow' is present
def parse_file(file_name):
    total_fct = 0
    num_flows = 0
    total_size = 0
    utilization = 0
    try:
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
    except IOError:
        print("Error: File %s does not appear to exist." % file_name)
        sys.exit(1)

    avg_fct = total_fct / num_flows
    avg_fct_ms = avg_fct / 1000
    total_size_mb = total_size / 1000000
    return utilization, avg_fct_ms, num_flows, total_size_mb


def get_output_file(input_file, output_dir):
    # extract parent directory name from input file name
    parent_dir_name = input_file.split("/")[-2]
    output_dir = output_dir + "/" + parent_dir_name
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    params = input_file.split("/")[-1].split("-")
    params_without_util = [param.split(".")[0] for param in params if "util" not in param]
    output_file = "-".join(params_without_util) + ".csv"
    output_file = output_dir + "/" + output_file

    return output_file

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python parse_results.py input_file output_dir")
        sys.exit(1)

    input_file = sys.argv[1]
    output_dir = sys.argv[2]

    output_file = get_output_file(input_file, output_dir)
    utilization, avg_fct_ms, num_flows, total_size_mb = parse_file(input_file)
    
    try:
        with open(output_file, 'a') as f:
            f.write("%s,%.2f,%s,%.2f\n" % (utilization, avg_fct_ms, num_flows, total_size_mb))
    except IOError:
        print("Error: File %s does not appear to exist." % output_file)
        sys.exit(1)


