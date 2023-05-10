#!/bin/bash

if [ $# -ne 5 ]
then
    echo "Usage: ./partition_sort.sh  <DATA SIZE (GB)> <NUM_WORKERS> <NUM_SERVERS> <USE_LD_PRELOAD> <SETUP_WORKER>"
    echo "Example: ./partition_sort.sh 1 2 2 true"
    exit 1
fi

DATA_SIZE_GB=$1
NUM_WORKERS=$2
NUM_SERVERS=$3
USE_LD_PRELOAD=$4
SETUP_WORKER=$5

REPO_DIR="/mnt/Work/Networks"
PROJECT_DIR="$REPO_DIR/Project/RDMA"
APP_DIR="$PROJECT_DIR/sorting"
LOG_DIR="$APP_DIR/logs"
HOSTS_FILE="~/hosts.txt"
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)

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

for ((i=1; i<=$NUM_SERVERS; i++))
do
    if [ $i -eq 1 ]
    then
        IP_LIST="$IP_PREFIX.$i"
    else
        IP_LIST="$IP_LIST,$IP_PREFIX.$i"
    fi
done

if [ $SETUP_WORKER = "true" ]
then
    for ((i=1; i<$NUM_SERVERS; i++))
    do
        echo "Setting up worker $i"
        ssh ${WORKER_PREFIX}-$i "cd $REPO_DIR; git checkout $CURRENT_BRANCH; git pull origin $CURRENT_BRANCH"
        
        ssh ${WORKER_PREFIX}-$i "mkdir -p $LOG_DIR"
    done
fi

for ((i=1; i<$NUM_SERVERS; i++))
do
    echo "Copying to worker $i"
    scp -rq $APP_DIR ${WORKER_PREFIX}-$i:$PROJECT_DIR
done

echo "Done copying to workers"

for ((i=0; i<$NUM_SERVERS; i++))
do
    echo "Running program on worker $i"
    APP_CMD="cd ${APP_DIR}; ./$SCRIPT_FILE $NUM_WORKERS $N $NUM_SERVERS $IP_LIST $TYPE $i $USE_LD_PRELOAD > ${LOG_DIR}/partition_sort_node-${i}.log 2>&1"
    echo "Running command: $APP_CMD"
    ssh ${WORKER_PREFIX}-$i "$APP_CMD" &
done
