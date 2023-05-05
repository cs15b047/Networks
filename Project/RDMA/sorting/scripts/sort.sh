make

rank_start=$1
rank_end=$2
workers=$3
N=$4
master_ip=$5

for ((i=$rank_start; i<=$rank_end; i++))
do
    ./sort $workers $N $master_ip $i &
done