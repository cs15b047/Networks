#!/bin/bash 

CURRENT_TIMESTAMP=$(date +%s)
LOG_DIR=logs/ts-$CURRENT_TIMESTAMP
FLOW_DISTS=("uniform" "pareto")
UTIL_START=0.1
UTIL_END=1.0
FLOW_SIZE=10000000


echo "Compiling htsim..."
make > /dev/null 

rc=$?
if [[ $rc != 0 ]] ; then
    exit $rc
fi

mkdir -p $LOG_DIR


for flowdist in ${FLOW_DISTS[@]}
do
    for utilization in $(seq $UTIL_START 0.1 $UTIL_END)
    do
        LOGFILE=$LOG_DIR/flow=$flowdist-size=$FLOW_SIZE-util=$utilization.log
        echo "Started experiment for flow distribution: $flowdist, utilization: $utilization, flow size: $FLOW_SIZE"
        ./htsim --expt=2 --utilization=$utilization --flowsize=$FLOW_SIZE --flowdist=$flowdist > $LOGFILE 2>&1 &
    done
done

echo "Output logs are in $LOG_DIR"

# Single experiment
# ./htsim --expt=2 --utilization=0.1 --flowsize=10000000 > $LOGFILE 2>&1