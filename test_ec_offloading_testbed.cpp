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
#include "ec_route.h"
#include "conga-topology.h"

namespace ec_offload {
    CongaTopology topo;
    int generateRandomRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst);
    int genDstRoutes(int fpga_id, std::vector<ec_route::flow_info_t> &flows);
}

using namespace std;
using namespace ec_offload;

void
ec_offload_testbed(const ArgList &args, Logfile &logfile)
{
    double Duration = 0.0001;
    double Utilization = 0.9;
    uint32_t AvgFlowSize = 4500;
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
    topo.genTopology(QueueType, logfile);
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

    FlowGenerator *bgFlowGen = new FlowGenerator(eh, generateRandomRoute, bg_flow_rate, AvgFlowSize, fd);
    bgFlowGen->setTimeLimits(timeFromUs(1), timeFromSec(Duration) - 1);

    EventList::Get().setEndtime(timeFromSec(Duration));

    // DataSource* src = new EcToFPGASrc(NULL, 7500);
    // DataSink* snk = new EcToFPGASink(500000, 0);
    // src->setName("src");
    // snk->setName("snk");
    // src->_node_id = 0;
    // snk->_node_id = 1;

    // auto route_fwd = new route_t();
    // auto route_rcv = new route_t();

    // auto queue_fwd = new Queue(40000000000, 1024000, NULL);
    // auto pipe_fwd = new Pipe(0.1);
    // auto queue_rcv = new Queue(40000000000, 1024000, NULL);
    // auto pipe_rcv = new Pipe(0.1);

    // route_fwd->push_back(queue_fwd);
    // route_fwd->push_back(pipe_fwd);
    // route_rcv->push_back(queue_rcv);
    // route_rcv->push_back(pipe_rcv);

    // route_fwd->push_back(snk);
    // route_rcv->push_back(src);

    // src->connect(EventList::Get().now() + 10, *route_fwd, *route_rcv, *snk);

}

// in FPGA/core offload case, flows go from client(src) to storage server(dst)
// modify src and dst here to change workload
// really sends flow to core; the route after reaching core is used as indicator when core issues forward flow
int
ec_offload::generateRandomRoute(route_t *&fwd,
                              route_t *&rev,
                              uint32_t &src,
                              uint32_t &core)    // core
{
    uint32_t dst = 0;   // need fix to match only server subset; any constraints?
    if (dst != 0) {
        dst = dst % CongaTopology::N_NODES;
    } else {
        dst = rand() % CongaTopology::N_NODES;
    }

    if (src != 0) {
        src = src % (CongaTopology::N_NODES - 1);
    } else {
        src = rand() % (CongaTopology::N_NODES - 1);
    }

    if (src >= dst) {
        src++;
    }

    if (core != 0) {
        core = core % CongaTopology::N_CORE;
    } else {
        core = rand() % CongaTopology::N_CORE;
    }

    uint32_t src_tree = src / CongaTopology::N_SERVER;
    uint32_t dst_tree = dst / CongaTopology::N_SERVER;
    uint32_t src_server = src % CongaTopology::N_SERVER;
    uint32_t dst_server = dst % CongaTopology::N_SERVER;

    fwd = new route_t();
    rev = new route_t();

    fwd->push_back(topo.qServerLeaf[src_tree][src_server]);
    fwd->push_back(topo.pServerLeaf[src_tree][src_server]);

    rev->push_back(topo.qServerLeaf[dst_tree][dst_server]);
    rev->push_back(topo.pServerLeaf[dst_tree][dst_server]);

    if (src_tree != dst_tree) {
        // core = get_best_lbtag(src, dst);
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

    return core;
}

// src node id is not used for offloading
int
ec_offload::genDstRoutes(int fpga_id, std::vector<ec_route::flow_info_t> &flows) {
// TODO
    for (int i = 2; i <= 4; i++) {
        auto route_fwd = new route_t();
        auto route_rcv = new route_t();

        auto queue_fwd = new Queue(40000000000, 1024000, NULL);
        auto pipe_fwd = new Pipe(0.1);
        auto queue_rcv = new Queue(40000000000, 1024000, NULL);
        auto pipe_rcv = new Pipe(0.1);

        route_fwd->push_back(queue_fwd);
        route_fwd->push_back(pipe_fwd);
        route_rcv->push_back(queue_rcv);
        route_rcv->push_back(pipe_rcv);

        flows.emplace_back(route_fwd, route_rcv, i);
    }

    // return 0;
    // uint32_t dst_tree = dst / CongaTopology::N_SERVER;
    // for (int i = ec_route::EC_K; i <= ec_route::EC_N; i++) {
    //     auto route_fwd = new route_t();
    //     auto route_rcv = new route_t();

    //     auto queue_fwd0 = topo.qCoreLeaf[fpga_id][dst_tree];//new Queue(10000000000, 8192000, NULL);
    //     auto pipe_fwd0 = topo.pCoreLeaf[fpga_id][dst_tree];//new Pipe(0.1);
    //     auto queue_fwd1 = topo.pLeafServer[dst_tree][dst];//new Queue(10000000000, 8192000, NULL);
    //     auto pipe_fwd1 = topo.pLeafServer[dst_tree][dst];//new Pipe(0.1);


    //     auto queue_rcv0 = topo.qServerLeaf[dst][dst_tree];//new Queue(10000000000, 8192000, NULL);
    //     auto pipe_rcv0 = topo.pServerLeaf[dst][dst_tree];//new Pipe(0.1);
    //     auto queue_rcv1 = topo.qLeafCore[dst_tree][fpga_id];//new Queue(10000000000, 8192000, NULL);
    //     auto pipe_rcv1 = topo.pLeafCore[dst_tree][fpga_id];//new Pipe(0.1);

    //     route_fwd->push_back(queue_fwd0);
    //     route_fwd->push_back(pipe_fwd0);
    //     route_fwd->push_back(queue_fwd1);
    //     route_fwd->push_back(pipe_fwd1);

    //     route_rcv->push_back(queue_rcv0);
    //     route_rcv->push_back(pipe_rcv0);
    //     route_rcv->push_back(queue_rcv1);
    //     route_rcv->push_back(pipe_rcv1);

    //     flows.emplace_back(route_fwd, route_rcv, i);
    // }


    return 0;
}
