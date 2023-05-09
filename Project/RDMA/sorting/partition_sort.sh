#!/bin/bash
make clean && make all

if [ $# -ne 7 ]
then
    echo "Usage: ./partition_sort.sh <num_workers> <N> <num_servers> <ip> <type> <server_id> <use_ld_preload>"
    echo "Example: ./partition_sort.sh 4 1000000 2"
    exit 1
fi

workers=$1
N=$2
num_servers=$3
ip=$4
type=$5
server_id=$6
use_ld_preload=$7
ld_preload_cmd="LD_PRELOAD=/usr/lib/libvma.so"

if [ $use_ld_preload = "false" ]
then
    ld_preload_cmd=""
fi

prog="kernel_sort"
if [ $type = "rdma" ]
then
    prog="rdma_sort"
    echo "Running RDMA sort"
fi

# Alternate between the two IPs
for ((i=0; i<$workers; i++))
do
    # Run if the server ID is the same as the rank % num_servers
    if [ $((i % num_servers)) -eq $server_id ]
    then
        sudo $ld_preload_cmd ./$prog $workers $N $ip $i &
    fi
done