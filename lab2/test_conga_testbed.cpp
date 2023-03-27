#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "aprx-fairqueue.h"
#include "fairqueue.h"
#include "priorityqueue.h"
#include "stoc-fairqueue.h"
#include "flow-generator.h"
#include "pipe.h"
#include "test.h"

#include <string>

using namespace std;

namespace conga {

    // tesdbed configuration
    const int N_CORE = 12;
    const int N_LEAF = 24;
    const int N_SERVER = 32;   // Per leaf
    const int N_NODES = N_SERVER * N_LEAF;

    const uint64_t LEAF_BUFFER = 512000;
    const uint64_t CORE_BUFFER = 1024000;
    const uint64_t ENDH_BUFFER = 8192000;

    const uint64_t LEAF_SPEED = 10000000000; // 10gbps
    const uint64_t CORE_SPEED = 40000000000; // 40gbps

    const double LINK_DELAY = 0.1;

    // Server <--> ToR
    // downlink
    Pipe  *pTorServer[N_LEAF][N_SERVER];
    Queue *qTorServer[N_LEAF][N_SERVER];
    // uplink
    Pipe  *pServerTor[N_LEAF][N_SERVER];
    Queue *qServerTor[N_LEAF][N_SERVER];

    // ToR <--> Core
    // downlink
    Pipe  *pTorCore[N_CORE][N_LEAF];
    Queue *qTorCore[N_CORE][N_LEAF];
    // uplink
    Pipe  *pCoreTor[N_CORE][N_LEAF];
    Queue *qCoreTor[N_CORE][N_LEAF];

    hash<string> hash_fn;

    Leafswitch *leafswitches[N_LEAF];

    string routing_algo;

    void createPath(vector<uint32_t>& path, route_t* &route);

    void generateRandomRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst);
    void createQueue(string &qType, Queue *&queue, uint64_t speed, uint64_t buffer, Logfile &logfile, Queue::QueueLocation location = Queue::QueueLocation::SRV_TOR, Leafswitch *leafswitch = NULL);

}

using namespace conga;


void buildTopology(string &qType, Logfile &logfile) {
    // Create leaf switches
    for (int i = 0; i < N_LEAF; i++) {
        leafswitches[i] = new Leafswitch(i, N_LEAF, N_CORE);
    }

    // Server <--> ToR
    for (int i = 0; i < N_LEAF; i++) {
        for (int j = 0; j < N_SERVER; j++) {
            // uplink
            pServerTor[i][j] = new Pipe(timeFromUs(LINK_DELAY));
            createQueue(qType, qServerTor[i][j], LEAF_SPEED, ENDH_BUFFER, logfile, Queue::QueueLocation::SRV_TOR);
            qServerTor[i][j]->setName("qServerTor[" + to_string(i) + "][" + to_string(j) + "]");
            pServerTor[i][j]->setName("pServerTor[" + to_string(i) + "][" + to_string(j) + "]");
            logfile.writeName(*(qServerTor[i][j]));
            logfile.writeName(*(pServerTor[i][j]));

            // downlink
            pTorServer[i][j] = new Pipe(timeFromUs(LINK_DELAY));
            createQueue(qType, qTorServer[i][j], LEAF_SPEED, LEAF_BUFFER, logfile, Queue::QueueLocation::TOR_SRV, leafswitches[i]);
            qTorServer[i][j]->setName("qTorServer[" + to_string(i) + "][" + to_string(j) + "]");
            pTorServer[i][j]->setName("pTorServer[" + to_string(i) + "][" + to_string(j) + "]");
            logfile.writeName(*(qTorServer[i][j]));
            logfile.writeName(*(pTorServer[i][j]));
        }
    }

    // ToR <--> Core
    for (int i = 0; i < N_CORE; i++) {
        for (int j = 0; j < N_LEAF; j++) {
            // uplink
            pTorCore[i][j] = new Pipe(timeFromUs(LINK_DELAY));
            createQueue(qType, qTorCore[i][j], CORE_SPEED, LEAF_BUFFER, logfile, Queue::QueueLocation::TOR_CORE, leafswitches[j]);
            qTorCore[i][j]->setName("qTorCore[" + to_string(i) + "][" + to_string(j) + "]");
            pTorCore[i][j]->setName("pTorCore[" + to_string(i) + "][" + to_string(j) + "]");
            logfile.writeName(*(qTorCore[i][j]));
            logfile.writeName(*(pTorCore[i][j]));

            // downlink
            pCoreTor[i][j] = new Pipe(timeFromUs(LINK_DELAY));
            createQueue(qType, qCoreTor[i][j], CORE_SPEED, CORE_BUFFER, logfile, Queue::QueueLocation::CORE_TOR);
            qCoreTor[i][j]->setName("qCoreTor[" + to_string(i) + "][" + to_string(j) + "]");
            pCoreTor[i][j]->setName("pCoreTor[" + to_string(i) + "][" + to_string(j) + "]");
            logfile.writeName(*(qCoreTor[i][j]));
            logfile.writeName(*(pCoreTor[i][j]));

        }
    }
}

Workloads::FlowDist getFlowDist(string &FlowDist) {
    Workloads::FlowDist fd;
    if (FlowDist == "pareto") {
        fd = Workloads::PARETO;
    } else if (FlowDist == "enterprise") {
        fd = Workloads::ENTERPRISE;
    } else if (FlowDist == "datamining") {
        fd = Workloads::DATAMINING;
    } else {
        fd = Workloads::UNIFORM;
    }
    return fd;
}


void
conga_testbed(const ArgList &args, Logfile &logfile)
{
    // testbed definition
    uint32_t Duration = 1;
    double Utilization = 0.1;
    uint32_t AvgFlowSize = 100000;
    // uint32_t Lstf = 0;
    string QueueType = "droptail";
    string EndHost = "tcp";
    string calq = "cq";
    string fairqueue = "fq";
    string FlowDist = "uniform";

    parseInt(args, "duration", Duration);
    parseInt(args, "flowsize", AvgFlowSize);
    // parseInt(args, "lstf", Lstf);
    parseDouble(args, "utilization", Utilization);
    parseString(args, "queue", QueueType);
    parseString(args, "endhost", EndHost);
    parseString(args, "flowdist", FlowDist);
    parseString(args, "algorithm", routing_algo);


    buildTopology(QueueType, logfile);
    cout << "Topology built" << endl;

    DataSource::EndHost eh = DataSource::TCP;
    Workloads::FlowDist flowDist = getFlowDist(FlowDist);
    double flow_rate = Utilization * (CORE_SPEED * N_CORE * N_LEAF);

    cout << "Starting flow generation" << endl;

    // generate flows
    FlowGenerator *bgFlowGen = new FlowGenerator(eh, generateRandomRoute, flow_rate, AvgFlowSize, flowDist);
    bgFlowGen->setTimeLimits(timeFromUs(1), timeFromMs(Duration) - 1);
    
    EventList::Get().setEndtime(timeFromMs(Duration));
}


void
conga::createQueue(string &qType,
                      Queue *&queue,
                      uint64_t speed,
                      uint64_t buffer,
                      Logfile &logfile,
                      Queue::QueueLocation location,
                      Leafswitch* leafswitch)
{
#if MING_PROF
    QueueLoggerSampling *qs = new QueueLoggerSampling(timeFromUs(100));
    //QueueLoggerSampling *qs = new QueueLoggerSampling(timeFromUs(10));
    //QueueLoggerSampling *qs = new QueueLoggerSampling(timeFromUs(50));
#else
    QueueLoggerSampling *qs = new QueueLoggerSampling(timeFromMs(10));
#endif
    logfile.addLogger(*qs);

    if (qType == "fq") {
        queue = new FairQueue(speed, buffer, qs);
    } else if (qType == "afq") {
        queue = new AprxFairQueue(speed, buffer, qs);
    } else if (qType == "pq") {
        queue = new PriorityQueue(speed, buffer, qs);
    } else if (qType == "sfq") {
        queue = new StocFairQueue(speed, buffer, qs);
    } else {
        queue = new Queue(speed, buffer, qs);
    }

    queue->_location = location;
    queue->_leafswitch = leafswitch;
}


void conga::createPath(vector<uint32_t>& path, route_t* &route) { // path: {srv, tor, core, tor, srv}
    assert(path.size() == 5);

    uint32_t srcSrv = path[0];
    uint32_t srcToR = path[1];
    uint32_t core = path[2];
    uint32_t dstToR = path[3];
    uint32_t dstSrv = path[4];

    // srv --> tor
    route->push_back(qServerTor[srcToR][srcSrv]);
    route->push_back(pServerTor[srcToR][srcSrv]);

    if(srcToR != dstToR) {
        // tor --> core
        route->push_back(qTorCore[core][srcToR]);
        route->push_back(pTorCore[core][srcToR]);

        // core --> tor
        route->push_back(qCoreTor[core][dstToR]);
        route->push_back(pCoreTor[core][dstToR]);
    }

    // tor --> srv
    route->push_back(qTorServer[dstToR][dstSrv]);
    route->push_back(pTorServer[dstToR][dstSrv]);
}


void conga::generateRandomRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst) {
    // generate random route
    src = rand() % N_NODES;
    dst = rand() % N_NODES;

    // if (dst != 0) {
    //     dst = dst % N_NODES;
    // } else {
    //     dst = rand() % N_NODES;
    // }

    // if (src != 0) {
    //     src = src % (N_NODES - 1);
    // } else {
    //     src = rand() % (N_NODES - 1);
    // }

    // if (src >= dst) {
    //     src++;
    // }


    uint32_t srcSrv = src % N_SERVER;
    uint32_t dstSrv = dst % N_SERVER;
    uint32_t srcToR = src / N_SERVER;
    uint32_t dstToR = dst / N_SERVER;

    uint32_t core = -1;
    if(routing_algo == "ecmp") {
        uint32_t ecmp_choice = hash_fn(to_string(src) + to_string(dst)) % N_CORE;
        core = ecmp_choice;
        // cout << "ecmp_choice: " << ecmp_choice << endl;
    } else if(routing_algo == "conga") {
        uint32_t conga_choice = INT_MAX;
        double min_ce = __DBL_MAX__;
        vector<uint32_t> min_choices;
        for(uint32_t i = 0; i < N_CORE; i++) {
            double ce = leafswitches[srcToR]->congestion_to_table[dstToR][i];

            if(ce < min_ce) {
                conga_choice = i;
                min_ce = ce;
                min_choices = {i};
            } else if (ce == min_ce) {
                min_choices.push_back(i);
            }
        }

        uint32_t ecmp_choice_idx = hash_fn(to_string(src) + to_string(dst)) % min_choices.size();
        conga_choice = min_choices[ecmp_choice_idx];

        // cout << "conga_choice: " << conga_choice << ", min CE: " << min_ce << endl;
        assert(conga_choice != INT_MAX); // something is picked!
        core = conga_choice;
    } else {
        assert(false);
    }

    fwd = new route_t();
    rev = new route_t();

    vector<uint32_t> path = {srcSrv, srcToR, core, dstToR, dstSrv};
    vector<uint32_t> revPath = {dstSrv, dstToR, core, srcToR, srcSrv};

    createPath(path, fwd);
    createPath(revPath, rev);

    for(auto& hop: *fwd) {
        hop->pkt_route = path;
    }

    for(auto& hop: *rev) {
        hop->pkt_route = revPath;
    }
}
