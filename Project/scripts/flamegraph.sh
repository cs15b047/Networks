if [ $# -lt 1 ]; then
    echo "Usage: $0 <binary path>"
    exit 1
fi

BASE_DIR=/mnt/Work/
PROJ_DIR=$BASE_DIR/Networks/Project
FLAME_DIR=$BASE_DIR/FlameGraph
PERF_LOG_DIR=$PROJ_DIR/logs

mkdir -p $PERF_LOG_DIR

cd $PROJ_DIR/DPDK/app
make > /dev/null
rc=$?
if [ $rc -ne 0 ]; then
    echo "make failed"
    exit
fi

cd $PROJ_DIR
killall fstack_client
sleep 2
${PROJ_DIR}/DPDK/app/build/fstack_client --conf ${PROJ_DIR}/DPDK/app/config.ini --proc-type=primary --proc-id=0 &
appPid=$!


echo "Recording..."
perf record -F 500 -p $appPid -g -- sleep 40

chmod 666 perf.data

echo "Stack traces"
perf script | $FLAME_DIR/stackcollapse-perf.pl > $PERF_LOG_DIR/out.perf-folded

echo "Plot..."
$FLAME_DIR/flamegraph.pl $PERF_LOG_DIR/out.perf-folded > $PERF_LOG_DIR/perf.svg
echo "Written to $PERF_LOG_DIR/perf.svg"