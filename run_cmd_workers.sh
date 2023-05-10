#!/bin/bash

if [ $# -ne 2 ]
then
    echo "Usage: ./run_cmd_workers.sh  <NUM_SERVERS> <CMD>"
    echo "Example: ./run_cmd_workers.sh 2 'sudo killall partition_sort'"
    exit 1
fi

NUM_SERVERS=$1
CMD=$2
WORKER_PREFIX="node"

for ((i=0; i<$NUM_SERVERS; i++))
do
    echo "Running $CMD on $WORKER_PREFIX-$i"
    ssh $WORKER_PREFIX-$i "$CMD" &
done



