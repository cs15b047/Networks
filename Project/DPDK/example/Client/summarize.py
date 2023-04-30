import os
import pandas as pd


BASE_DIR="/users/ajsj7598/Networks/lab1/Client/Measurement/"

multi_flow_bw = "multiflow-bandwidth"
single_flow_bw = "singleflow-bandwidth"
single_flow_latency = "singleflow-latency"

# read all files with prefix overall_latency in single_flow_latency directory and get the average of all values
def get_single_flow_latency():
    files = os.listdir(BASE_DIR + single_flow_latency)
    files = [f for f in files if f.startswith("overall_latency")]
    overall_latency = {}
    for file in files:
        df = pd.read_csv(BASE_DIR + single_flow_latency + "/" + file, header="infer")
        for f in df['Flow Size']:
            if f not in overall_latency:
                overall_latency[f] = []
            overall_latency[f].append(df[df['Flow Size'] == f][' Overall Latency'].values[0])
    return overall_latency

# read all files with prefix per_packet_latency in single_flow_latency directory and get the average of all values
def get_single_flow_latency_per_packet():
    files = os.listdir(BASE_DIR + single_flow_latency)
    files = [f for f in files if f.startswith("per_packet_latency")]
    per_packet_latency_avg = {}
    per_packet_latency_min = {}
    per_packet_latency_max = {}

    for file in files:
        df = pd.read_csv(BASE_DIR + single_flow_latency + "/" + file, header="infer")
        for f in df['Flow Size']:
            if f not in per_packet_latency_avg:
                per_packet_latency_avg[f] = []
                per_packet_latency_min[f] = []
                per_packet_latency_max[f] = []

            per_packet_latency_avg[f].append(df[df['Flow Size'] == f][' Latency (avg)'].values[0])
            per_packet_latency_min[f].append(df[df['Flow Size'] == f][' Latency (min)'].values[0])
            per_packet_latency_max[f].append(df[df['Flow Size'] == f][' Latency (max)'].values[0])
    return per_packet_latency_avg, per_packet_latency_min, per_packet_latency_max

# read all files with prefix bandwidth in single_flow_bw directory and get the average of all values
def get_single_flow_bandwidth():
    files = os.listdir(BASE_DIR + single_flow_bw)
    files = [f for f in files if f.startswith("bandwidth")]
    bandwidth = {}
    for file in files:
        df = pd.read_csv(BASE_DIR + single_flow_bw + "/" + file, header="infer")
        for f in df['Flow Size']:
            if f not in bandwidth:
                bandwidth[f] = []
            bandwidth[f].append(df[df['Flow Size'] == f][' Bandwidth'].values[0])
    return bandwidth


# read all files with prefix multbandwidth in multi_flow_bw directory and get the average of all values
def get_multi_flow_bandwidth():
    files = os.listdir(BASE_DIR + multi_flow_bw)
    files = [f for f in files if f.startswith("multbandwidth")]
    bandwidth = {}
    for file in files:
        df = pd.read_csv(BASE_DIR + multi_flow_bw + "/" + file, header="infer")
        for f in df['Flow Num']:
            if f not in bandwidth:
                bandwidth[f] = []
            bandwidth[f].append(df[df['Flow Num'] == f][' Bandwidth'].values[-1])
    return bandwidth


def filter_dict(d, keys):
    return {k: d[k] for k in keys if k in d}

def get_metrics(d):
    return {k: (sum(d[k]) / len(d[k]), min(d[k]), max(d[k])) for k in d.keys() if k in d}

def write_dict_to_csv(d, filename, print_metrics = True):
    results_dict = {}
    if print_metrics:
        results_dict = get_metrics(d)
    else:
        results_dict = d
    df = pd.DataFrame.from_dict(results_dict, orient='index')
    df.to_csv(filename, header=False)

latency_flow_sizes = [64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384]
bandwidth_flow_sizes = [int(pow(2, f)) for f in range(30, 36)]

overall_latency = get_single_flow_latency()
overall_latency = filter_dict(overall_latency, latency_flow_sizes)
write_dict_to_csv(overall_latency, "overall_latency.csv")
# print(overall_latency)


per_packet_latency_avg, per_packet_latency_min, per_packet_latency_max = get_single_flow_latency_per_packet()
per_packet_latency_avg = filter_dict(per_packet_latency_avg, latency_flow_sizes)
per_packet_latency_min = filter_dict(per_packet_latency_min, latency_flow_sizes)
per_packet_latency_max = filter_dict(per_packet_latency_max, latency_flow_sizes)

write_dict_to_csv(per_packet_latency_avg, "per_packet_latency_avg.csv")
write_dict_to_csv(per_packet_latency_min, "per_packet_latency_min.csv")
write_dict_to_csv(per_packet_latency_max, "per_packet_latency_max.csv")

# print(per_packet_latency_avg)

bandwidth = get_single_flow_bandwidth()
bandwidth = filter_dict(bandwidth, bandwidth_flow_sizes)
write_dict_to_csv(bandwidth, "bandwidth.csv")
# print(bandwidth)

multibandwidth = get_multi_flow_bandwidth()
write_dict_to_csv(multibandwidth, "multibandwidth.csv")