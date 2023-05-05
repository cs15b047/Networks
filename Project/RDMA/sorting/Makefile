rdma_sort: sort.cpp | build
	g++ -o build/rdma_sort sort.cpp rdma_common.c -Ofast -lrdmacm -libverbs -lpthread

kernel_sort: kernel_sort.cpp | build
	g++ -o build/kernel_sort kernel_sort.cpp -Ofast -lpthread

all: rdma_sort kernel_sort

build:
	mkdir -p build

clean:
	rm -rf build
