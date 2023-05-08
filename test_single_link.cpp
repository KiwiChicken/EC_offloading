#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "aprx-fairqueue.h"
#include "stoc-fairqueue.h"
#include "fairqueue.h"
#include "flow-generator.h"
#include "pipe.h"
#include "test.h"

namespace linksim {
    route_t routeFwd;
    route_t routeRev;

    void generateRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst);
}

using namespace std;
using namespace linksim;

void single_link_simulation(const ArgList &args, Logfile &logfile)
{
    uint32_t Duration = 10;           // Experiment duration
    uint64_t LinkSpeed = 10000000000; // 10Gbps
    uint64_t LinkDelay = 25;          // 10 mircoseconds
    uint64_t LinkBuffer = 512000;     // Buffer size
    uint32_t MaxFlows = 0;            // Max concurrent flows (ignores utilization)
    uint32_t AvgFlowSize = 100000;    // Average flow size.
    string FlowDist = "uniform";      // Flow Distribution.
    double Utilization = 0.75;        // How loaded to run the link
    double OnOffRatio = 0.0;          // ON-OFF ration (if MaxFlows != 0).
    string QueueType = "droptail";    // Queue type (droptail/fq/afq)
    string EndHost = "tcp";           // Endhost type (tcp/pp)
    string Trace = "";                // File containing trace to replay.
    struct AFQcfg afqcfg;             // AFQ config.

    parseInt(args, "duration", Duration);
    parseLongInt(args, "linkspeed", LinkSpeed);
    parseLongInt(args, "linkdelay", LinkDelay);
    parseLongInt(args, "linkbuffer", LinkBuffer);
    parseDouble(args, "utilization", Utilization);
    parseDouble(args, "onoff", OnOffRatio);
    parseInt(args, "maxflows", MaxFlows);
    parseInt(args, "flowsize", AvgFlowSize);
    parseString(args, "flowdist", FlowDist);
    parseString(args, "queue", QueueType);
    parseString(args, "endhost", EndHost);
    parseString(args, "trace", Trace);
    parseInt(args, "afqH", afqcfg.nHash);
    parseInt(args, "afqB", afqcfg.nBucket);
    parseInt(args, "afqQ", afqcfg.nQueue);
    parseInt(args, "afqBpR", afqcfg.bytesPerRound);
    parseInt(args, "afqAlpha", afqcfg.alpha);

    QueueLoggerSampling *qs = new QueueLoggerSampling(timeFromUs(10));
    logfile.addLogger(*qs);

    TcpLoggerSimple *logTcp = new TcpLoggerSimple();
    logfile.addLogger(*logTcp);

    // Build the network
    Pipe *pipeFwd = new Pipe(timeFromUs(LinkDelay/2));
    pipeFwd->setName("pipeFwd");
    logfile.writeName(*pipeFwd);

    Pipe *pipeRev = new Pipe(timeFromUs(LinkDelay/2));
    pipeRev->setName("pipeRev");
    logfile.writeName(*pipeRev);

    Queue *queueFwd;
    if (QueueType == "fq") {
        queueFwd = new FairQueue(LinkSpeed, LinkBuffer, qs);
    } else if (QueueType == "afq") {
        queueFwd = new AprxFairQueue(LinkSpeed, LinkBuffer, qs, afqcfg);
    } else if (QueueType == "sfq") {
        queueFwd = new StocFairQueue(LinkSpeed, LinkBuffer, qs);
    } else {
        queueFwd = new Queue(LinkSpeed, LinkBuffer, qs);
    }

    queueFwd->setName("queueFwd");
    logfile.writeName(*queueFwd);

    Queue *queueRev = new Queue(LinkSpeed, LinkBuffer, NULL);
    queueRev ->setName("queueRev");
    logfile.writeName(*queueRev);

    routeFwd.push_back(queueFwd);
    routeFwd.push_back(pipeFwd);

    routeRev.push_back(queueRev);
    routeRev.push_back(pipeRev);

    DataSource::EndHost eh = DataSource::TCP;
    Workloads::FlowDist fd  = Workloads::UNIFORM;

    if (EndHost == "pp") {
        eh = DataSource::PKTPAIR;
    } else if (EndHost == "timely") {
        eh = DataSource::TIMELY;
    } else if (EndHost == "dctcp") {
        eh = DataSource::DCTCP;
    }

    if (FlowDist == "pareto") {
        fd = Workloads::PARETO;
    } else if (FlowDist == "enterprise") {
        fd = Workloads::ENTERPRISE;
    } else if (FlowDist == "datamining") {
        fd = Workloads::DATAMINING;
    }

    /* Configure flow generator. */
    linkspeed_bps flowRate = llround(LinkSpeed * Utilization);
    if (MaxFlows != 0) {
        flowRate = LinkSpeed;
    }

    FlowGenerator *flowGen = new FlowGenerator(eh, generateRoute, flowRate, AvgFlowSize, fd);

    if (MaxFlows != 0) {
        flowGen->setReplaceFlow(MaxFlows, OnOffRatio);
    }

    if (Trace != "") {
        flowGen->setTrace(Trace);
    }

    flowGen->setEndhostQueue(LinkSpeed, 8192000);
    flowGen->setTimeLimits(0, timeFromSec(Duration) - 1);


    EventList::Get().setEndtime(timeFromSec(Duration));
}

void
linksim::generateRoute(route_t *&fwd, route_t *&rev, uint32_t &src, uint32_t &dst)
{
    fwd = new route_t(routeFwd);
    rev = new route_t(routeRev);
    src = 0;
    dst = 1;
}
