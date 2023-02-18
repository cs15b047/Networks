#!/bin/bash
make clean
make  > /dev/null 2>&1
ret=$?
if [ $ret -ne 0 ]; then
    echo "Build failed"
    exit 1
fi

FLOW_SIZE_START_SM=$((2**6))
FLOW_SIZE_END_SM=$((2**14))
FLOW_SIZE_START_LG=$((1024*1024*1024))
FLOW_SIZE_END_LG=$((32*1024*1024*1024))
NUM_ITERS=5

BASE_DIR="/users/ajsj7598/Networks/lab1/Client/Measurement"
RUNNER_EXE="/users/ajsj7598/Networks/lab1/Client/build/lab1-client"

LATENCY_HEADER="Flow Size, Window Size, Latency (avg), Latency (max), Latency (min)"
OVERALL_LATENCY_HEADER="Flow Size, Window Size, Overall Latency"
BANDWIDTH_HEADER="Flow Size, Window Size, Bandwidth"
MULTI_FLOW_BANDWIDTH_HEADER="Flow Num, Window Size, Bandwidth"

# Make directories for storing results
mkdir -p $BASE_DIR/singleflow-latency
mkdir -p $BASE_DIR/singleflow-bandwidth
mkdir -p $BASE_DIR/multiflow-bandwidth

for (( ITER=0; ITER<$NUM_ITERS; ITER++ ))
do
    LATENCY_STATS="$BASE_DIR/singleflow-latency/per_packet_latency_iter_$ITER.csv" 
    OVERALL_LATENCY_STATS="$BASE_DIR/singleflow-latency/overall_latency_iter_$ITER.csv" 
    BANDWIDTH_STATS="$BASE_DIR/singleflow-bandwidth/bandwidth_iter_$ITER.csv" 
    MULTI_FLOW_BANDWIDTH_STATS="$BASE_DIR/multiflow-bandwidth/multbandwidth_iter_$ITER.csv" 

    echo $LATENCY_HEADER > $LATENCY_STATS
    echo $OVERALL_LATENCY_HEADER > $OVERALL_LATENCY_STATS
    echo $BANDWIDTH_HEADER > $BANDWIDTH_STATS
    echo $MULTI_FLOW_BANDWIDTH_HEADER > $MULTI_FLOW_BANDWIDTH_STATS
done

echo "Running single flow latency tests"
TCP_WINDOW_LEN=20
FLOW_NUM=1
for (( FLOW_SIZE=$FLOW_SIZE_START_SM; FLOW_SIZE<=$FLOW_SIZE_END_SM; FLOW_SIZE*=2 ))
do
   for (( ITER=0; ITER<$NUM_ITERS; ITER++ ))
    do
        echo "Running with flow size $FLOW_SIZE, iteration $ITER"
        sudo $RUNNER_EXE ${FLOW_NUM} ${FLOW_SIZE} ${TCP_WINDOW_LEN} ${BASE_DIR} ${ITER} > /dev/null 2>&1
    done
done


echo "Running single flow bandwidth tests"
TCP_WINDOW_LEN=20
FLOW_NUM=1
for (( FLOW_SIZE=$FLOW_SIZE_START_LG; FLOW_SIZE<=$FLOW_SIZE_END_LG; FLOW_SIZE*=2 ))
do
   for (( ITER=0; ITER<$NUM_ITERS; ITER++ ))
    do
        echo "Running with flow size $FLOW_SIZE, iteration $ITER"
        sudo $RUNNER_EXE ${FLOW_NUM} ${FLOW_SIZE} ${TCP_WINDOW_LEN} ${BASE_DIR} ${ITER} > /dev/null 2>&1
    done
done



echo "Running multi flow bandwidth tests"
TCP_WINDOW_LEN=20
MAX_FLOWS=8
FLOW_SIZE=$((100*1024*1024))

for (( FLOW_NUM=1; FLOW_NUM<=$MAX_FLOWS; FLOW_NUM+=1 ))
do
    for (( ITER=0; ITER<$NUM_ITERS; ITER++ ))
    do
        echo "Running with flow num $FLOW_NUM, iteration $ITER"
        sudo $RUNNER_EXE ${FLOW_NUM} ${FLOW_SIZE} ${TCP_WINDOW_LEN} ${BASE_DIR} ${ITER} > /dev/null 2>&1
    done
done