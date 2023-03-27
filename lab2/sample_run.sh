make > /dev/null

utilization=0.9
FLOW_SIZE=10000000
flowdist="uniform"
ALGOS=("conga" "ecmp")
duration_ms=100
TS=`date +%s`
LOG_DIR=logs/ts-$TS
mkdir -p $LOG_DIR

for algo in ${ALGOS[@]}
do
    echo "Started experiment for flow distribution: $flowdist, utilization: $utilization, flow size: $FLOW_SIZE, algorithm: $algo"
    LOGFILE=$LOG_DIR/sample_algo=$algo-flowdist=$flowdist.log
    ./htsim --expt=2 --utilization=$utilization --flowsize=$FLOW_SIZE --flowdist=$flowdist --algorithm=$algo --duration=$duration_ms > $LOGFILE 2>&1 &
done

echo "Writing results to $LOG_DIR"