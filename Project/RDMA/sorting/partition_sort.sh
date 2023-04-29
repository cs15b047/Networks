make clean && make

workers=$1
N=$2
rank_start=$3
rank_end=$4
ip="10.10.1.3,10.10.1.2"
# Alternate between the two IPs
for ((i=$rank_start; i<=$rank_end; i++))
do
    ./sort $workers $N $ip $i &
done