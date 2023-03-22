make

rc=$?
if [[ $rc != 0 ]] ; then
    exit $rc
fi

OUTFILE=run.log
./htsim --expt=2 --utilization=0.1 --flowsize=10000000 > $OUTFILE 2>&1