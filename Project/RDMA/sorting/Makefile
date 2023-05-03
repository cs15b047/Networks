rdma_sort: sort.cpp
	g++ -o rdma_sort sort.cpp rdma_common.c -Ofast -lrdmacm -libverbs -lpthread

kernel_sort: kernel_sort.cpp
	g++ -o kernel_sort kernel_sort.cpp -g -lpthread

all: rdma_sort kernel_sort

clean:
	rm -f rdma_sort kernel_sort
