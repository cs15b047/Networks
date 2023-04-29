make clean && make

workers=$1
N=$2
num_servers=$3
ip=$4
server_id=$5
# Alternate between the two IPs
for ((i=0; i<$workers; i++))
do
    # Run if the server ID is the same as the rank % num_servers
    if [ $((i % num_servers)) -eq $server_id ]
    then
        ./sort $workers $N $ip $i &
    fi
done