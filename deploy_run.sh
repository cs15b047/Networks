#!/bin/bash

if [ $# -ne 4 ]
then
    echo "Usage: ./partition_sort.sh  <DATA SIZE (GB)> <NUM_WORKERS> <NUM_SERVERS> <USE_LD_PRELOAD>"
    echo "Example: ./partition_sort.sh 1 2 2 true"
    exit 1
fi

DATA_SIZE_GB=$1
NUM_WORKERS=$2
NUM_SERVERS=$3
USE_LD_PRELOAD=$4

BASE_DIR="/mnt/Work/Networks/Project/RDMA"
APP_DIR="$BASE_DIR/sorting"
LOG_DIR="$APP_DIR/logs"

WORKER_PREFIX="node"
TYPE="kernel"
LD_PRELOAD_CMD="LD_PRELOAD=/usr/lib/libvma.so"
SCRIPT_FILE="partition_sort.sh"

RECORD_SIZE_BYTES=100
N=$((DATA_SIZE_GB*1024*1024*1024/RECORD_SIZE_BYTES))

IP_PREFIX="10.10.1"
IP_LIST=""

if [ $USE_LD_PRELOAD = "false" ]
then
    LD_PRELOAD_CMD=""
fi

for ((i=0; i<$NUM_SERVERS; i++))
do
    if [ $i -eq 0 ]
    then
        IP_LIST="$IP_PREFIX.$i"
    else
        IP_LIST="$IP_LIST,$IP_PREFIX.$i"
    fi
done

for ((i=1; i<$NUM_SERVERS; i++))
do
    echo "Setting up worker $i"
    scp -rq $APP_DIR ${WORKER_PREFIX}-$i:$BASE_DIR
    ssh ${WORKER_PREFIX}-$i "mkdir -p $LOG_DIR"
done

echo "Done copying to workers"

for ((i=0; i<$NUM_SERVERS; i++))
do
    echo "Running program on worker $i"
    APP_CMD="${APP_DIR}/$SCRIPT_FILE $NUM_WORKERS $N $NUM_SERVERS $IP_LIST $TYPE $i $USE_LD_PRELOAD > ${LOG_DIR}/partition_sort_node-${i}.log"
    ssh ${WORKER_PREFIX}-$i "$APP_CMD" &
done