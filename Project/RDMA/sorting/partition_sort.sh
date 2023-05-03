make clean && make all

workers=$1
N=$2
num_servers=$3
ip=$4
type=$5
server_id=$6

if $type -eq "rdma"
then
    prog="rdma_sort"
else
    prog="kernel_sort"
fi

# Alternate between the two IPs
for ((i=0; i<$workers; i++))
do
    # Run if the server ID is the same as the rank % num_servers
    if [ $((i % num_servers)) -eq $server_id ]
    then
        ./$prog $workers $N $ip $i &
    fi
done