#!/bin/bash



worker_addr=("node-0" "node-1" "node-2")
own_addr="node-0"
DIR="/mnt/Work/Networks/Project/RDMA"

if [ $# -eq 1 ]
then
    DIR=$1
fi

for addr in ${worker_addr[@]}
do
    if [ $addr != $own_addr ]
    then
        echo "Copying to $addr"
        scp -rq $DIR $addr:$DIR
        if [ $? -ne 0 ]
        then
            echo "Copy failed to $addr"
            exit 1
        fi
    fi
done


# Build the project
APP_DIR="$DIR/sorting"
for addr in ${worker_addr[@]}
do
    echo "Building on $addr"
    ssh $addr "cd $APP_DIR; make clean; make" > /dev/null 
    if [ $? -ne 0 ]
    then
        echo "Build failed on $addr"
        exit 1
    fi
done