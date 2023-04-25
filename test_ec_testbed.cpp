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

namespace ec {
    CongaTopology topo;
    int generateRandomRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst);
}

using namespace std;
using namespace ec;

void
conga_testbed(const ArgList &args, Logfile &logfile)
{
    double Duration = 0.1;
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
    topo.genTopology(QueueType, logfile);

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

// in base case, flows directly go from client(src) to storage server(dst)
// modify src and dst here to change workload
int
ec::generateRandomRoute(route_t *&fwd,
                              route_t *&rev,
                              uint32_t &src,
                              uint32_t &dst)
{
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

    uint32_t src_tree = src / CongaTopology::N_SERVER;
    uint32_t dst_tree = dst / CongaTopology::N_SERVER;
    uint32_t src_server = src % CongaTopology::N_SERVER;
    uint32_t dst_server = dst % CongaTopology::N_SERVER;
    uint32_t core = rand() % CongaTopology::N_CORE; // same return core for consistency

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
