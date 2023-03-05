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

    const uint64_t LEAF_BUFFER = 512000;
    const uint64_t CORE_BUFFER = 1024000;
    const uint64_t ENDH_BUFFER = 8192000;

    const uint64_t LEAF_SPEED = 10000000000; // 10gbps
    const uint64_t CORE_SPEED = 40000000000; // 40gbps

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

    // Leafswitch *leafswitches[N_LEAF];


    void createPath(vector<uint32_t>& path, route_t* &route);

    void generateRandomRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst);
    void createQueue(string &qType, Queue *&queue, uint64_t speed, uint64_t buffer, Logfile &logfile);

}

using namespace conga;


void buildTopology(string &qType, Logfile &logfile) {
    // Server <--> ToR
    for (int i = 0; i < N_LEAF; i++) {
        for (int j = 0; j < N_SERVER; j++) {
            // uplink
            pServerTor[i][j] = new Pipe(timeFromUs(1));
            createQueue(qType, qServerTor[i][j], LEAF_SPEED, ENDH_BUFFER, logfile);
            qServerTor[i][j]->setName("qServerTor[" + to_string(i) + "][" + to_string(j) + "]");
            pServerTor[i][j]->setName("pServerTor[" + to_string(i) + "][" + to_string(j) + "]");
            logfile.writeName(*(qServerTor[i][j]));
            logfile.writeName(*(pServerTor[i][j]));

            // downlink
            pTorServer[i][j] = new Pipe(timeFromUs(1));
            createQueue(qType, qTorServer[i][j], LEAF_SPEED, LEAF_BUFFER, logfile);
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
            pTorCore[i][j] = new Pipe(timeFromUs(1));
            createQueue(qType, qTorCore[i][j], CORE_SPEED, LEAF_BUFFER, logfile);
            qTorCore[i][j]->setName("qTorCore[" + to_string(i) + "][" + to_string(j) + "]");
            pTorCore[i][j]->setName("pTorCore[" + to_string(i) + "][" + to_string(j) + "]");
            logfile.writeName(*(qTorCore[i][j]));
            logfile.writeName(*(pTorCore[i][j]));

            // downlink
            pCoreTor[i][j] = new Pipe(timeFromUs(1));
            createQueue(qType, qCoreTor[i][j], CORE_SPEED, CORE_BUFFER, logfile);
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
    uint32_t Duration = 2;
    double Utilization = 0.9;
    uint32_t AvgFlowSize = 100000;
    uint32_t Lstf = 0;
    string QueueType = "droptail";
    string EndHost = "dctcp";
    string calq = "cq";
    string fairqueue = "fq";
    string FlowDist = "uniform";

    parseInt(args, "duration", Duration);
    parseInt(args, "flowsize", AvgFlowSize);
    parseInt(args, "lstf", Lstf);
    parseDouble(args, "utilization", Utilization);
    parseString(args, "queue", QueueType);
    parseString(args, "endhost", EndHost);
    parseString(args, "flowdist", FlowDist);


    buildTopology(QueueType, logfile);
    cout << "Topology built" << endl;
    DataSource::EndHost eh = DataSource::TCP;

    Workloads::FlowDist flowDist = getFlowDist(FlowDist);
    double flow_rate = (LEAF_SPEED / 1e4) * Utilization; // TODO: Set to a reasonable value

    cout << "Starting flow generation" << endl;

    // generate flows
    FlowGenerator *bgFlowGen = new FlowGenerator(eh, generateRandomRoute, flow_rate, AvgFlowSize, flowDist);
    bgFlowGen->setTimeLimits(timeFromUs(1), timeFromSec(Duration) - 1);
    
    EventList::Get().setEndtime(timeFromSec(Duration));
}


void
conga::createQueue(string &qType,
                      Queue *&queue,
                      uint64_t speed,
                      uint64_t buffer,
                      Logfile &logfile)
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

    // tor --> core
    route->push_back(qTorCore[core][srcToR]);
    route->push_back(pTorCore[core][srcToR]);

    // core --> tor
    route->push_back(qCoreTor[core][dstToR]);
    route->push_back(pCoreTor[core][dstToR]);

    // tor --> srv
    route->push_back(qTorServer[dstToR][dstSrv]);
    route->push_back(pTorServer[dstToR][dstSrv]);
}


void conga::generateRandomRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst) {
    // generate random route
    src = rand() % (N_SERVER * N_LEAF);
    dst = rand() % (N_SERVER * N_LEAF);


    uint32_t srcSrv = src % N_SERVER;
    uint32_t dstSrv = dst % N_SERVER;
    uint32_t srcToR = src / N_SERVER;
    uint32_t dstToR = dst / N_SERVER;

    // Choose a random core switch --> ECMP?
    uint32_t core = rand() % N_CORE;

    fwd = new route_t();
    rev = new route_t();

    vector<uint32_t> path = {srcSrv, srcToR, core, dstToR, dstSrv};
    vector<uint32_t> revPath = {dstSrv, dstToR, core, srcToR, srcSrv};

    createPath(path, fwd);
    createPath(revPath, rev);
}
