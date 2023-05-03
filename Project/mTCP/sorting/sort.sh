
# Check if the number of arguments is correct
if [ $# -ne 2 ]
then
    echo "Usage: ./sort.sh <workers> <N>"
    exit 1
fi

workers=$1
N=$2

make clean && make
ret=$?
if [ $ret -ne 0 ]
then
    echo "Compilation failed"
    exit 1
fi

for ((i=1; i<$workers; i++))
do
    ./sort $workers $N $i &
done