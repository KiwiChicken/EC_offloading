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
    int generateRandomRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst);
    int genDstRoutes(int fpga_id, std::vector<ec_route::flow_info_t> &flows);
}

using namespace std;
using namespace ec_raw;

void
ec_testbed(const ArgList &args, Logfile &logfile)
{
    double Duration = 0.0001;
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

    // Aggregation to core switches and vice-versa.
    topo.genTopology(false, QueueType, logfile);
    ec_route::genRoutes = &genDstRoutes;

    DataSource::EndHost eh = DataSource::TCP;
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

    FlowGenerator *bgFlowGen = new FlowGenerator(eh, generateRandomRoute, bg_flow_rate, AvgFlowSize, fd);
    bgFlowGen->setTimeLimits(timeFromUs(1), timeFromSec(Duration) - 1);

    EventList::Get().setEndtime(timeFromSec(Duration));
}


int
ec_raw::generateRandomRoute(route_t *&fwd,
                              route_t *&rev,
                              uint32_t &src,
                              uint32_t &dst)
{
    // assert(src == 0);
    if (dst != 0) {
        dst = dst % (CongaTopology::N_NODES - CongaTopology::N_LEAF); // FIXME: confirm if dst is even going to be used without offloading
    } else {
        dst = rand() % (CongaTopology::N_NODES - CongaTopology::N_LEAF);
    }
    // src default is 0!
    if (src != 0) {
        src = src % (CongaTopology::N_NODES - 1);
    } else {
        // src = rand() % (N_NODES - 1);
    }

    uint32_t src_tree = src / CongaTopology::N_SERVER;
    uint32_t dst_tree = dst / CongaTopology::N_SERVER;
    
    if (dst_tree == src_tree) {
        dst+=CongaTopology::N_LEAF;
    }

    // colocated src and fpga: empty route
    fwd = new route_t();
    rev = new route_t();

    // if (dst != 0) {
    //     dst = dst % CongaTopology::N_NODES;
    // } else {
    //     dst = rand() % CongaTopology::N_NODES;
    // }

    // if (src != 0) {
    //     src = src % (CongaTopology::N_NODES - 1);
    // } else {
    //     src = rand() % (CongaTopology::N_NODES - 1);
    // }

    // if (src >= dst) {
    //     src++;
    // }

    // uint32_t src_tree = src / CongaTopology::N_SERVER;
    // uint32_t dst_tree = dst / CongaTopology::N_SERVER;
    // uint32_t src_server = src % CongaTopology::N_SERVER;
    // uint32_t dst_server = dst % CongaTopology::N_SERVER;
    // uint32_t core = rand() % CongaTopology::N_CORE; // same return core for consistency

    // fwd = new route_t();
    // rev = new route_t();

    // fwd->push_back(topo.qServerLeaf[src_tree][src_server]);
    // fwd->push_back(topo.pServerLeaf[src_tree][src_server]);

    // rev->push_back(topo.qServerLeaf[dst_tree][dst_server]);
    // rev->push_back(topo.pServerLeaf[dst_tree][dst_server]);

    // if (src_tree != dst_tree) {
    //     fwd->push_back(topo.qLeafCore[core][src_tree]);
    //     fwd->push_back(topo.pLeafCore[core][src_tree]);

    //     rev->push_back(topo.qLeafCore[core][dst_tree]);
    //     rev->push_back(topo.pLeafCore[core][dst_tree]);

    //     fwd->push_back(topo.qCoreLeaf[core][dst_tree]);
    //     fwd->push_back(topo.pCoreLeaf[core][dst_tree]);

    //     rev->push_back(topo.qCoreLeaf[core][src_tree]);
    //     rev->push_back(topo.pCoreLeaf[core][src_tree]);
    // }

    // fwd->push_back(topo.qLeafServer[dst_tree][dst_server]);
    // fwd->push_back(topo.pLeafServer[dst_tree][dst_server]);

    // rev->push_back(topo.qLeafServer[src_tree][src_server]);
    // rev->push_back(topo.pLeafServer[src_tree][src_server]);

    return 0;
}


// ASSUME: src and dst are not from the same group(default src_id = 0); fpga_id = src_id
int
ec_raw::genDstRoutes(int fpga_id, std::vector<ec_route::flow_info_t> &flows) {
    uint32_t dst = rand() % (CongaTopology::N_NODES - CongaTopology::N_LEAF); // FIXME: which subset are dst servers?
    uint32_t src = fpga_id % (CongaTopology::N_NODES - 1);

    

    uint32_t src_tree = src / CongaTopology::N_SERVER;
    uint32_t dst_tree = dst / CongaTopology::N_SERVER;
    uint32_t src_server = src % CongaTopology::N_SERVER;
    uint32_t dst_server = dst % CongaTopology::N_SERVER;
    uint32_t core = rand() % CongaTopology::N_CORE; // same return core for consistency
    
    if (dst_tree == src_tree) {
        dst+=CongaTopology::N_LEAF;
    }

    for (int i = ec_route::EC_K; i < ec_route::EC_N; i++) {  // FIXME: k or 0?
        auto route_fwd = new route_t();
        auto route_rcv = new route_t();
        // same as original
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
    }


    // for (int i = 0; i < ec_route::EC_N; i++) {
    //     auto route_fwd = new route_t();
    //     auto route_rcv = new route_t();

    //     auto queue_fwd = topo.qServerLeaf[0][0];//new Queue(10000000000, 8192000, NULL);
    //     auto pipe_fwd = topo.pServerLeaf[0][0];//new Pipe(0.1);
    //     auto queue_rcv = topo.qServerLeaf[0][0];//new Queue(10000000000, 8192000, NULL);
    //     auto pipe_rcv = topo.pServerLeaf[0][0];//new Pipe(0.1);

    //     route_fwd->push_back(queue_fwd);
    //     route_fwd->push_back(pipe_fwd);
    //     route_rcv->push_back(queue_rcv);
    //     route_rcv->push_back(pipe_rcv);

    //     flows.emplace_back(route_fwd, route_rcv, i);
    // }


    return core;
}