#ifndef SORT_UTILS_H
#define SORT_UTILS_H

#include <bits/stdc++.h>
using namespace std;

#define RECORD_SIZE 100

struct Record{
    int key;
    char value[RECORD_SIZE - sizeof(int)];

    bool operator<(const Record& other) const {
        return key < other.key;
    }
    bool operator>(const Record& other) const {
        return key > other.key;
    }
};

void generate_random_input(vector<Record>& partition, int partition_size) {
    // Generate random input
    generate(partition.begin(), partition.begin() + partition_size, []() {
        struct Record r;
        r.key = rand() % INT_MAX;
        return r;
    });
}

void print_partition(vector<Record>& partition) {
    for(int i = 0; i < partition.size(); i++)
        cout << i << " -> " << partition[i].key << endl;
}

void merge(vector<Record>& merged_arr, vector<int>& partition_sizes, int num_workers, vector<Record>& result) {
    long N = merged_arr.size();
    vector<long> partition_ptrs(num_workers);
    vector<long> cum_partition_sizes(num_workers);
    cum_partition_sizes[0] = partition_sizes[0];
    for(int i = 1; i < num_workers; i++) {
        cum_partition_sizes[i] = cum_partition_sizes[i - 1] + partition_sizes[i];
    }
    assert(cum_partition_sizes[num_workers - 1] == N);

    // assign pointers to the start of each partition
    partition_ptrs[0] = 0;
    for(int i = 1; i < num_workers; i++) {
        partition_ptrs[i] = cum_partition_sizes[i - 1];
    }

    // Perform a k-way merge (k = num_workers) using extra space
    for(long i = 0; i < N; i++) {
        int min_val = INT_MAX;
        int min_idx = -1;
        // get the minimum value from each partition
        for(int j = 0; j < num_workers; j++) {
            long ptr = partition_ptrs[j];
            if(ptr < cum_partition_sizes[j] && merged_arr[ptr].key <= min_val) {
                min_val = merged_arr[ptr].key;
                min_idx = j;
            }
        }
        result[i] = merged_arr[partition_ptrs[min_idx]];
        partition_ptrs[min_idx]++;
    }
}

bool verify_sorted(vector<Record>& arr) {
    int sz = arr.size();
    for(int i = 1; i < sz; i++) {
        if(arr[i - 1] > arr[i]) {
            cout << "Erring element: idx: " << i << " --> " << arr[i - 1].key << " " << arr[i].key << endl;
            return false;
        }
    }
    return true;
}

bool verify_partitioning(vector<int>& partition_sizes, long N) {
    long sum = 0;
    int num_workers = partition_sizes.size();
    cout << "Num workers: " <<  num_workers << endl;
    for(int i = 0; i < num_workers; i++) {
        sum += (long)partition_sizes[i];
    }
    // cout << "Sum of sizes: " << sum << ", Expected size: " << N << endl;
    return sum == N;
}

void partition_data(vector<Record>& data, vector<Record*>& partition_starts, vector<int>& partition_sizes, int num_workers) {
    partition_starts.resize(num_workers, NULL);
    partition_sizes.resize(num_workers, 0);

    int range_span = INT_MAX / num_workers;

    // range for worker i: [i * range_span, (i + 1) * range_span)
    // for worker num_workers - 1: [i * range_span, INT_MAX]
    int itr = 0;
    for(int i = 0; i < num_workers; i++) {
        int lower = i * range_span;
        int upper = (i == num_workers - 1) ? INT_MAX : (i + 1) * range_span;
        partition_starts[i] = &(data.data()[itr]);
        if(i == num_workers - 1) {
            partition_sizes[i] = (int)data.size() - itr;
        } else {
            const Record upper_ele = Record{upper, ""};
            partition_sizes[i] = upper_bound(data.begin() + itr, data.end(), upper_ele) - (data.begin() + itr);
        }
        itr += partition_sizes[i];
    }
}


/////////////////////////////////////////////////////////////////////////////////////////


#endif