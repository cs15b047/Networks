#!/bin/bash

# check if the number of arguments is correct
if [ $# -ne 2 ]; then
    echo "Usage: $0 <log_dir> <results_dir>"
    exit 1
fi

LOG_DIR=$1
RESULTS_DIR=$2
mkdir -p $RESULTS_DIR

# loop all log files in the directory
for file in $LOG_DIR/*.log
do
    echo "Parsing $file"
    python parse_results.py $file $RESULTS_DIR
done