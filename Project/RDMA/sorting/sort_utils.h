#ifndef SORT_UTILS_H
#define SORT_UTILS_H

#include <bits/stdc++.h>
using namespace std;

void generate_random_input(vector<int>& partition, int partition_size) {
    // Generate random input
    generate(partition.begin(), partition.begin() + partition_size, []() {
        return rand() % INT_MAX;
    });
}

void print_partition(vector<int>& partition) {
    for(int i = 0; i < partition.size(); i++)
        cout << i << " -> " << partition[i] << endl;
}

void merge(vector<int>& merged_arr, vector<int>& partition_sizes, int num_workers, vector<int>& result) {
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
            if(ptr < cum_partition_sizes[j] && merged_arr[ptr] <= min_val) {
                min_val = merged_arr[ptr];
                min_idx = j;
            }
        }
        result[i] = min_val;
        partition_ptrs[min_idx]++;
    }
}

bool verify_sorted(vector<int>& arr) {
    int sz = arr.size();
    for(int i = 1; i < sz; i++) {
        if(arr[i - 1] > arr[i]) {
            cout << "Erring element: idx: " << i << " --> " << arr[i - 1] << " " << arr[i] << endl;
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

void partition_data(vector<int>& data, vector<int*>& partition_starts, vector<int>& partition_sizes, int num_workers) {
    partition_starts.resize(num_workers, NULL);
    partition_sizes.resize(num_workers, 0);

    int range_span = INT_MAX / num_workers;

    // TODO: Remove this
    uint64_t total_size = data.size(), partition_size = total_size / num_workers;
    for(int i = 0; i < num_workers; i++) {
        partition_starts[i] = &(data.data()[i * partition_size]);
        if(i == num_workers - 1) {
            partition_sizes[i] = total_size - i * partition_size;
        } else {
            partition_sizes[i] = partition_size;
        }
    }
    return;

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
            partition_sizes[i] = upper_bound(data.begin() + itr, data.end(), upper) - (data.begin() + itr);
        }
        itr += partition_sizes[i];
    }
}

#endif