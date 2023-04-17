make clean && make

workers=$1
N=$2
master_ip=$3

for ((i=1; i<$workers; i++))
do
    ./sort $workers $N $master_ip $i &
done