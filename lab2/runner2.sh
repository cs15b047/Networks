#!/bin/bash 

CURRENT_TIMESTAMP=$(date +%s)
LOG_DIR=logs/ts-$CURRENT_TIMESTAMP
FLOW_DISTS=("uniform")
ALGOS=("conga")
UTIL_START=0.1
UTIL_END=1.0
FLOW_SIZE=50000


echo "Compiling htsim..."
make > /dev/null 

rc=$?
if [[ $rc != 0 ]] ; then
    exit $rc
fi

mkdir -p $LOG_DIR


duration_ms=100
for flowdist in ${FLOW_DISTS[@]}
do
    for utilization in $(seq $UTIL_START 0.1 $UTIL_END)
    do
        # TODO: Put loop over (conga, ecmp)
        for algo in ${ALGOS[@]}
        do
            LOGFILE=$LOG_DIR/flow=$flowdist-size=$FLOW_SIZE-util=$utilization-algo=$algo.log
            echo "Started experiment for flow distribution: $flowdist, utilization: $utilization, flow size: $FLOW_SIZE, algorithm: $algo"
            # TODO: Add algorithm as conga as well
            ./htsim --expt=2 --utilization=$utilization --flowsize=$FLOW_SIZE --flowdist=$flowdist --algorithm=$algo --duration=$duration_ms > $LOGFILE 2>&1 &
        done
    done
done

while pgrep htsim > /dev/null
do
    sleep 1
done

echo "Output logs are in $LOG_DIR"

# Single experiment
# ./htsim --expt=2 --utilization=0.1 --flowsize=10000000 > $LOGFILE 2>&1