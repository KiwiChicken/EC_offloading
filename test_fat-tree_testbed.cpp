/*
 * FatTree experiment
 */
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
#include "prof.h"

namespace fat_tree {
    const int N_SUBTREE = 4;  // In full network
    const int N_TOR = 2;      // Per SubTree
    const int N_AGG = 2;      // Per SubTree
    const int N_UPLINK = 2;   // From Agg to Core
    const int N_SERVER = 32;  // Per ToR

    // Total number of nodes in the topology
    const int N_NODES = N_SUBTREE * N_TOR * N_SERVER;
    const int N_NODES_SUBTREE = N_TOR * N_SERVER;

    const uint64_t ENDH_BUFFER       = 8192000;
    const uint64_t TOR_SERVER_BUFFER = 512000;
    const uint64_t TOR_AGG_BUFFER    = 1024000;
    const uint64_t AGG_TOR_BUFFER    = 1024000;
    const uint64_t AGG_CORE_BUFFER   = 1024000;
    const uint64_t CORE_AGG_BUFFER   = 1024000;

    const uint64_t SERVER_TOR_SPEED = 10000000000ULL;   // 10gbps
    const uint64_t TOR_AGG_SPEED    = 40000000000ULL;   // 40gbps
    const uint64_t AGG_CORE_SPEED   = 40000000000ULL;   // 40gbps

    const double LINK_DELAY = 0.1; // in microsec

    Pipe  *pCoreAgg[N_SUBTREE][N_AGG][N_UPLINK];
    Queue *qCoreAgg[N_SUBTREE][N_AGG][N_UPLINK];

    Pipe  *pAggCore[N_SUBTREE][N_AGG][N_UPLINK];
    Queue *qAggCore[N_SUBTREE][N_AGG][N_UPLINK];

    Pipe  *pAggTor[N_SUBTREE][N_AGG][N_TOR];
    Queue *qAggTor[N_SUBTREE][N_AGG][N_TOR];

    Pipe  *pTorAgg[N_SUBTREE][N_AGG][N_TOR];
    Queue *qTorAgg[N_SUBTREE][N_AGG][N_TOR];

    Pipe  *pTorServer[N_SUBTREE][N_TOR][N_SERVER];
    Queue *qTorServer[N_SUBTREE][N_TOR][N_SERVER];

    Pipe  *pServerTor[N_SUBTREE][N_TOR][N_SERVER];
    Queue *qServerTor[N_SUBTREE][N_TOR][N_SERVER];

    void generateRandomRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst);
    void createQueue(std::string &qType, Queue *&queue, uint64_t speed, uint64_t buffer, Logfile &lf);
}

using namespace std;
using namespace fat_tree;

void
fat_tree_testbed(const ArgList &args, 
                 Logfile &logfile)
{
    uint32_t Duration = 5;
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

    // Aggregation to core switches and vice-versa.
    for (int i = 0; i < N_SUBTREE; i++) {
        for (int j = 0; j < N_AGG; j++) {
            for (int k = 0; k < N_UPLINK; k++) {
                // Uplink
                createQueue(QueueType, qAggCore[i][j][k], AGG_CORE_SPEED, AGG_CORE_BUFFER, logfile);
                qAggCore[i][j][k]->setName("q-agg-core-" + to_string(i) + "-" + to_string(j) + "-" + to_string(k));
                logfile.writeName(*(qAggCore[i][j][k]));

                pAggCore[i][j][k] = new Pipe(timeFromUs(LINK_DELAY));
                pAggCore[i][j][k]->setName("p-agg-core-" + to_string(i) + "-" + to_string(j) + "-" + to_string(k));
                logfile.writeName(*(pAggCore[i][j][k]));

                // Downlink
                createQueue(QueueType, qCoreAgg[i][j][k], AGG_CORE_SPEED, CORE_AGG_BUFFER, logfile);
                qCoreAgg[i][j][k]->setName("q-core-agg-" + to_string(i) + "-" + to_string(j) + "-" + to_string(k));
                logfile.writeName(*(qCoreAgg[i][j][k]));

                pCoreAgg[i][j][k] = new Pipe(timeFromUs(LINK_DELAY));
                pCoreAgg[i][j][k]->setName("p-core-agg-" + to_string(i) + "-" + to_string(j) + "-" + to_string(k));
                logfile.writeName(*(pCoreAgg[i][j][k]));
            }
        }
    }

    // ToR to Aggregation switches and vice-versa.
    for (int i = 0; i < N_SUBTREE; i++) {
        for (int j = 0; j < N_AGG; j++) {
            for (int k = 0; k < N_TOR; k++) {
                // Uplink
                createQueue(QueueType, qTorAgg[i][j][k], TOR_AGG_SPEED, TOR_AGG_BUFFER, logfile);
                qTorAgg[i][j][k]->setName("q-tor-agg-" + to_string(i) + "-" + to_string(j) + "-" + to_string(k));
                logfile.writeName(*(qTorAgg[i][j][k]));

                pTorAgg[i][j][k] = new Pipe(timeFromUs(LINK_DELAY));
                pTorAgg[i][j][k]->setName("p-tor-agg-" + to_string(i) + "-" + to_string(j) + "-" + to_string(k));
                logfile.writeName(*(pTorAgg[i][j][k]));

                // Downlink
                createQueue(QueueType, qAggTor[i][j][k], TOR_AGG_SPEED, AGG_TOR_BUFFER, logfile);
                qAggTor[i][j][k]->setName("q-agg-tor-" + to_string(i) + "-" + to_string(j) + "-" + to_string(k));
                logfile.writeName(*(qAggTor[i][j][k]));

                pAggTor[i][j][k] = new Pipe(timeFromUs(LINK_DELAY));
                pAggTor[i][j][k]->setName("p-agg-tor-" + to_string(i) + "-" + to_string(j) + "-" + to_string(k));
                logfile.writeName(*(pAggTor[i][j][k]));
            }
        }
    }

    // Server to ToR switches and vice-versa.
    for (int i = 0; i < N_SUBTREE; i++) {
        for (int j = 0; j < N_TOR; j++) {
            for (int k = 0; k < N_SERVER; k++) {
                // Uplink
                createQueue(fairqueue, qServerTor[i][j][k], SERVER_TOR_SPEED, ENDH_BUFFER, logfile);
                qServerTor[i][j][k]->setName("q-server-tor-" + to_string(i) + "-" + to_string(j) + "-" + to_string(k));
                logfile.writeName(*(qServerTor[i][j][k]));

                pServerTor[i][j][k] = new Pipe(timeFromUs(LINK_DELAY));
                pServerTor[i][j][k]->setName("p-server-tor-" + to_string(i) + "-" + to_string(j) + "-" + to_string(k));
                logfile.writeName(*(pServerTor[i][j][k]));

                // Downlink
                createQueue(QueueType, qTorServer[i][j][k], SERVER_TOR_SPEED, TOR_SERVER_BUFFER, logfile);
                qTorServer[i][j][k]->setName("q-tor-server-" + to_string(i) + "-" + to_string(j) + "-" + to_string(k));
                logfile.writeName(*(qTorServer[i][j][k]));

                pTorServer[i][j][k] = new Pipe(timeFromUs(LINK_DELAY));
                pTorServer[i][j][k]->setName("p-tor-server-" + to_string(i) + "-" + to_string(j) + "-" + to_string(k));
                logfile.writeName(*(pTorServer[i][j][k]));
            }
        }
    }

    DataSource::EndHost eh = DataSource::TCP;
    DataSource::EndHost cfeh = DataSource::TCP;
    Workloads::FlowDist fd  = Workloads::UNIFORM;

    if (EndHost == "pp") {
        eh = DataSource::PKTPAIR;
    } else if (EndHost == "timely") {
        eh = DataSource::TIMELY;
    } else if (EndHost == "dctcp") {
        eh = DataSource::DCTCP;
    } else if (EndHost == "dtcp") {
        eh = DataSource::D_TCP;
    } else if (EndHost == "ddctcp") {
        eh = DataSource::D_DCTCP;
    }

    if (FlowDist == "pareto") {
        fd = Workloads::PARETO;
    } else if (FlowDist == "enterprise") {
        fd = Workloads::ENTERPRISE;
    } else if (FlowDist == "datamining") {
        fd = Workloads::DATAMINING;
    } else {
        fd = Workloads::UNIFORM;
    }

    if (Lstf == 0) {
        cfeh = DataSource::DCTCP;
    } else {
        cfeh = DataSource::D_DCTCP;
    }

    // Calculate background traffic utilization.
    double bg_flow_rate = Utilization * (TOR_AGG_SPEED * N_SUBTREE * N_AGG * N_UPLINK);

    // Adjust for traffic not exiting the ToR.
    bg_flow_rate = bg_flow_rate * (N_SUBTREE * N_TOR) / (N_SUBTREE * N_TOR - 1);

    // Create space for deadline/coflow traffic.
    //bg_flow_rate = 0.5 * bg_flow_rate;

    // Calculate deadline traffic rate.
    //double deadline_flow_rate = 0.25 * bg_flow_rate;
    //double deadline_flow_rate = bg_flow_rate;

    FlowGenerator *bgFlowGen = new FlowGenerator(eh, generateRandomRoute, bg_flow_rate, AvgFlowSize, fd);
    bgFlowGen->setTimeLimits(timeFromUs(1), timeFromSec(Duration) - 1);

    //CoflowGenerator *deadlineFlowGen = new CoflowGenerator(cfeh, generateRandomRoute, deadline_flow_rate);
    //deadlineFlowGen->setTimeLimits(timeFromUs(1), timeFromSec(Duration) - 1);
    //deadlineFlowGen->setPrefix("deadline");

    EventList::Get().setEndtime(timeFromSec(Duration));
}

void
fat_tree::generateRandomRoute(route_t *&fwd,
                              route_t *&rev,
                              uint32_t &src,
                              uint32_t &dst)
{
    if (dst != 0) {
        dst = dst % N_NODES;
    } else {
        dst = rand() % N_NODES;
    }

    if (src != 0) {
        src = src % (N_NODES - 1);
    } else {
        src = rand() % (N_NODES - 1);
    }

    if (src >= dst) {
        src++;
    }

    uint32_t src_tree = src / N_NODES_SUBTREE;
    uint32_t dst_tree = dst / N_NODES_SUBTREE;
    uint32_t uplink   = rand() % N_UPLINK;
    uint32_t src_agg  = rand() % N_AGG;
    uint32_t dst_agg  = src_agg;
    uint32_t src_tor  = (src / N_SERVER) % N_TOR;
    uint32_t dst_tor  = (dst / N_SERVER) % N_TOR;
    uint32_t src_svr  = src % N_SERVER;
    uint32_t dst_svr  = dst % N_SERVER;

    fwd = new route_t();
    rev = new route_t();

    fwd->push_back(qServerTor[src_tree][src_tor][src_svr]);
    fwd->push_back(pServerTor[src_tree][src_tor][src_svr]);

    rev->push_back(qServerTor[dst_tree][dst_tor][dst_svr]);
    rev->push_back(pServerTor[dst_tree][dst_tor][dst_svr]);

    if (src_tree != dst_tree || src_tor != dst_tor) {
        fwd->push_back(qTorAgg[src_tree][src_agg][src_tor]);
        fwd->push_back(pTorAgg[src_tree][src_agg][src_tor]);

        rev->push_back(qTorAgg[dst_tree][dst_agg][dst_tor]);
        rev->push_back(pTorAgg[dst_tree][dst_agg][dst_tor]);

        if (src_tree != dst_tree) {
            fwd->push_back(qAggCore[src_tree][src_agg][uplink]);
            fwd->push_back(pAggCore[src_tree][src_agg][uplink]);

            rev->push_back(qAggCore[dst_tree][dst_agg][uplink]);
            rev->push_back(pAggCore[dst_tree][dst_agg][uplink]);

            fwd->push_back(qCoreAgg[dst_tree][dst_agg][uplink]);
            fwd->push_back(pCoreAgg[dst_tree][dst_agg][uplink]);

            rev->push_back(qCoreAgg[src_tree][src_agg][uplink]);
            rev->push_back(pCoreAgg[src_tree][src_agg][uplink]);
        }

        fwd->push_back(qAggTor[dst_tree][dst_agg][dst_tor]);
        fwd->push_back(pAggTor[dst_tree][dst_agg][dst_tor]);

        rev->push_back(qAggTor[src_tree][src_agg][src_tor]);
        rev->push_back(pAggTor[src_tree][src_agg][src_tor]);
    }

    fwd->push_back(qTorServer[dst_tree][dst_tor][dst_svr]);
    fwd->push_back(pTorServer[dst_tree][dst_tor][dst_svr]);

    rev->push_back(qTorServer[src_tree][src_tor][src_svr]);
    rev->push_back(pTorServer[src_tree][src_tor][src_svr]);
}

void
fat_tree::createQueue(string &qType,
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
