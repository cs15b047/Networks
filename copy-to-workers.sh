workers=("node-1" "node-2")
BASE_DIR="/mnt/Work/Networks/Project/RDMA"
APP_DIR="$BASE_DIR/sorting"

for worker in "${workers[@]}"
do
    echo "Copying to $worker"
    scp -rq $APP_DIR $worker:$BASE_DIR
done