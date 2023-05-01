#!/bin/bash

ARRAY_SIZE=1000

# check if array size is provided as an argument
if [ $# -eq 1 ]; then
    ARRAY_SIZE=$1
fi

# This script is used to run the sorting application on the DPDK-enabled machine.
SSH_MACHINES=("node-0" "node-1" "node-2")
APP_DIR="/mnt/Work/Networks/Project/DPDK/sorting"
APP_NAME="./build/sort"

# Compile the application in each machine and verify that it was compiled successfully.
for machine in "${SSH_MACHINES[@]}"
do
    echo "Compiling the application on $machine"
    ssh $machine "cd $APP_DIR && make" 
    if [ $? -ne 0 ]; then
        echo "Failed to compile the application on $machine"
        exit 1
    fi
done

# Kill any running instances of the application.
for machine in "${SSH_MACHINES[@]}"
do
    echo "Killing any running instances of the application on $machine"
    EXE_NAME=$(echo $APP_NAME | cut -d'/' -f3)
    ssh $machine "sudo pkill $EXE_NAME"
done

# Run the application on each machine.
for machine in "${SSH_MACHINES[@]}"
do
    rank=$(echo $machine | cut -d'-' -f2)
        echo "Running the application on $machine with array size $ARRAY_SIZE and rank $rank"
        ssh $machine "cd $APP_DIR && sudo $APP_NAME $ARRAY_SIZE $rank" &
    if [ $rank -eq 0 ]; then
        sleep 1
    fi
done