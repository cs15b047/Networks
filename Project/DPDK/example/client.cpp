#include "client.h"


vector<int64_t> data;
int64_t data_len;

void generate_random_data() {
    srand(time(NULL));
    for (int64_t i = 0; i < data_len; i++) {
        data[i] = rand() % 256;
    }
}

static void ClientStart() {
    send_partition(data);
    print_stats();
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: ./client <flow size gb>>\n");
        return -1;
    }
    
    data_len = (int64_t)atoi(argv[1]);
    data.resize(data_len);
    generate_random_data();
   
    int ret = ClientSetup(argc, argv, data_len);
    if (ret < 0) {
        printf("Error setting up server\n");
        return ret;
    }
    ClientStart();
    ClientStop();
    return 0;
}
