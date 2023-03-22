#ifndef LEAFSWITCH_H
#define LEAFSWITCH_H

#include <vector>

using namespace std;

class Leafswitch {
public:
    Leafswitch(int id, int num_tor, int num_core) {
        _id = id;
        _num_tor = num_tor;
        _num_core = num_core;
        congestion_from_table.resize(num_tor);
        congestion_to_table.resize(num_tor);
        for (int i = 0; i < num_tor; i++) {
            congestion_from_table[i].resize(num_core, 0.0);
            congestion_to_table[i].resize(num_core, 0.0);
        }
    }
    vector<vector<double>> congestion_from_table, congestion_to_table;
private:
    int _id;
    int _num_tor, _num_core;
    

};

#endif /* LEAFSWITCH_H */