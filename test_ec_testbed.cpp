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
#include "conga-topology.h"

namespace ec_raw {
    CongaTopology topo;
    int genDstRoutes(int fpga_id, std::vector<ec_route::flow_info_t> &flows);
    
    int genEcRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst);
    int genSmallFlowRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst);
}

using namespace std;
using namespace ec_raw;

void
ec_testbed(const ArgList &args, Logfile &logfile)
{
    double Duration = 0.01;
    double EcChance = 1.0;
    double Utilization = 0.9;
    uint32_t AvgFlowSize = 100000;
    string QueueType = "droptail";
    string calq = "cq";
    string fairqueue = "fq";
    string FlowDist = "uniform";

    parseDouble(args, "duration", Duration);
    parseInt(args, "flowsize", AvgFlowSize);
    parseDouble(args, "utilization", Utilization);
    parseString(args, "queue", QueueType);
    parseString(args, "flowdist", FlowDist);
    parseDouble(args, "ecchance", EcChance);

    ec_route::ec_exec_time = 0;
    ec_route::fpga_mem_limit = 0;

    // uint32_t Ec_K = 7;
    // uint32_t Ec_N = 12;
    // parseInt(args, "K", Ec_K);
    // parseInt(args, "N", Ec_N);

    // ec_route::EC_K = (int)Ec_K;
    // ec_route::EC_N = (int)Ec_N;

    // Aggregation to core switches and vice-versa.
    topo.genTopology(false, QueueType, logfile);
    ec_route::genRoutes = &genDstRoutes;
    DataSource::EndHost eh = DataSource::EC_OFFLOAD;
    Workloads::FlowDist fd  = Workloads::ERASURE_CODING;

    if (FlowDist == "pareto") {
        fd = Workloads::PARETO;
    } else if (FlowDist == "enterprise") {
        fd = Workloads::ENTERPRISE;
    } else if (FlowDist == "datamining") {
        fd = Workloads::DATAMINING;
    }  else if (FlowDist == "erasure_coding") {
        fd = Workloads::ERASURE_CODING;
    } else {
        fd = Workloads::UNIFORM;
    }

    // Calculate background traffic utilization.
    double bg_flow_rate = Utilization * (CongaTopology::LEAF_SPEED * CongaTopology::N_SERVER * CongaTopology::N_LEAF);

    if (EcChance > 0) {
        double scaling = 1.0 * ec_route::EC_K / ec_route::EC_N;
        FlowGenerator *ecFlowGen = new FlowGenerator(eh, genEcRoute, bg_flow_rate * EcChance * scaling, AvgFlowSize, fd);
        ecFlowGen->setTimeLimits(timeFromUs(1), timeFromSec(Duration) - 1);
    }

    if (EcChance < 1) {
        FlowGenerator *smallFlowGen = new FlowGenerator(DataSource::TCP, genSmallFlowRoute, bg_flow_rate * (1-EcChance), AvgFlowSize, fd);
        smallFlowGen->setTimeLimits(timeFromUs(1), timeFromSec(Duration) - 1);
    }

    EventList::Get().setEndtime(timeFromSec(Duration));

    // route_t *routeFwd = NULL, *routeRev = NULL;
    // uint32_t src_node = 0, dst_node = 0;
    // genEcRoute(routeFwd, routeRev, src_node, dst_node);

    // DataSource *src = new EcToFPGASrc(NULL, 4500);
    // DataSink *snk = new EcToFPGASink(dst_node);

    // auto start_time = EventList::Get().now() + 10;

    // src->setName("src");
    // snk->setName("snk");
    // src->_node_id = src_node;
    // snk->_node_id = dst_node;

    // src->setDeadline(start_time + timeFromSec((4500 * 8.0) / speedFromGbps(0.8)));

    // routeFwd->push_back(snk);
    // routeRev->push_back(src);
    // src->connect(start_time, *routeFwd, *routeRev, *snk);
}

int
ec_raw::genSmallFlowRoute(route_t *&fwd,
                        route_t *&rev,
                        uint32_t &src,
                        uint32_t &dst)
{
    if (src == 0)
        src = rand();
    if (dst == 0)
        dst = rand();
 
    src = src % CongaTopology::N_NODES;
    dst = dst % (CongaTopology::N_NODES - 1);

    if (src == dst) {
        dst += 1;
    }

    uint32_t src_tree = src / CongaTopology::N_SERVER;
    uint32_t dst_tree = dst / CongaTopology::N_SERVER;
    uint32_t src_server = src % CongaTopology::N_SERVER;
    uint32_t dst_server = dst % CongaTopology::N_SERVER;
    uint32_t core = rand() % CongaTopology::N_CORE; // same return core for consistency
    
    fwd = new route_t();
    rev = new route_t();
    // do not use FPGA queue/pipes, use host's

    fwd->push_back(topo.qServerLeaf[src_tree][src_server]);
    fwd->push_back(topo.pServerLeaf[src_tree][src_server]);

    rev->push_back(topo.qServerLeaf[dst_tree][dst_server]);
    rev->push_back(topo.pServerLeaf[dst_tree][dst_server]);

    if (src_tree != dst_tree) {
        fwd->push_back(topo.qLeafCore[core][src_tree]);
        fwd->push_back(topo.pLeafCore[core][src_tree]);

        rev->push_back(topo.qLeafCore[core][dst_tree]);
        rev->push_back(topo.pLeafCore[core][dst_tree]);

        fwd->push_back(topo.qCoreLeaf[core][dst_tree]);
        fwd->push_back(topo.pCoreLeaf[core][dst_tree]);

        rev->push_back(topo.qCoreLeaf[core][src_tree]);
        rev->push_back(topo.pCoreLeaf[core][src_tree]);
    }

    fwd->push_back(topo.qLeafServer[dst_tree][dst_server]);
    fwd->push_back(topo.pLeafServer[dst_tree][dst_server]);

    rev->push_back(topo.qLeafServer[src_tree][src_server]);
    rev->push_back(topo.pLeafServer[src_tree][src_server]);
    
    return 0;
}

int
ec_raw::genEcRoute(route_t *&fwd,
                    route_t *&rev,
                    uint32_t &src,
                    uint32_t &dst)
{
    if (src == 0)
        src = rand();
    // colocated src and fpga: empty route3
    src = src % CongaTopology::N_NODES;
    dst = src;
    fwd = new route_t();
    rev = new route_t();

    return 0;
}


// ASSUME: src and dst are not from the same group(default src_id = 0); fpga_id = src_id
int
ec_raw::genDstRoutes(int src_id, std::vector<ec_route::flow_info_t> &flows) {

    uint32_t src = src_id % CongaTopology::N_NODES;
    uint32_t src_tree = src / CongaTopology::N_SERVER;
    uint32_t src_server = src % CongaTopology::N_SERVER;

    for (int i = 0; i < ec_route::EC_N; i++) {
    
        uint32_t dst = rand() % (CongaTopology::N_NODES - 1); // FIXME: which subset are dst servers?
        if (dst == src)
            dst += 1;

        uint32_t dst_tree = dst / CongaTopology::N_SERVER;
        uint32_t dst_server = dst % CongaTopology::N_SERVER;
        uint32_t core = rand() % CongaTopology::N_CORE; // same return core for consistency
        
        auto route_fwd = new route_t();
        auto route_rcv = new route_t();
        // do not use FPGA queue/pipes, use host's

        route_fwd->push_back(topo.qServerLeaf[src_tree][src_server]);
        route_fwd->push_back(topo.pServerLeaf[src_tree][src_server]);

        route_rcv->push_back(topo.qServerLeaf[dst_tree][dst_server]);
        route_rcv->push_back(topo.pServerLeaf[dst_tree][dst_server]);

        if (src_tree != dst_tree) {
            route_fwd->push_back(topo.qLeafCore[core][src_tree]);
            route_fwd->push_back(topo.pLeafCore[core][src_tree]);

            route_rcv->push_back(topo.qLeafCore[core][dst_tree]);
            route_rcv->push_back(topo.pLeafCore[core][dst_tree]);

            route_fwd->push_back(topo.qCoreLeaf[core][dst_tree]);
            route_fwd->push_back(topo.pCoreLeaf[core][dst_tree]);

            route_rcv->push_back(topo.qCoreLeaf[core][src_tree]);
            route_rcv->push_back(topo.pCoreLeaf[core][src_tree]);
        }

        route_fwd->push_back(topo.qLeafServer[dst_tree][dst_server]);
        route_fwd->push_back(topo.pLeafServer[dst_tree][dst_server]);

        route_rcv->push_back(topo.qLeafServer[src_tree][src_server]);
        route_rcv->push_back(topo.pLeafServer[src_tree][src_server]);

        flows.emplace_back(route_fwd, route_rcv, i);
        
        // cout << "genDstRoutes: src " << src << "; FPGA_id " << fpga_id << "; dst " << dst << endl;
    }

    return 0;
}