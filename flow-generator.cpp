/*
 * Flow generator
 */
#include "flow-generator.h"

using namespace std;

FlowGenerator::FlowGenerator(DataSource::EndHost endhost, 
                             route_gen_t rg,
                             linkspeed_bps flowRate, 
                             uint32_t avgFlowSize, 
                             Workloads::FlowDist flowSizeDist)
    : EventSource("FlowGen"),
    _prefix(""),
    _endhost(endhost),
    _routeGen(rg),
    _flowRate(flowRate),
    _flowSizeDist(flowSizeDist),
    _flowsGenerated(0),
    _workload(avgFlowSize, flowSizeDist),
    _endhostQ(false),
    _useTrace(false),
    _replaceFlow(false),
    _maxFlows(0),
    _concurrentFlows(0),
    _avgOffTime(0),
    _flowTrace(),
    _liveFlows()
{
    double flowsPerSec = _flowRate / (_workload._avgFlowSize * 8.0);
    _avgFlowArrivalTime = timeFromSec(1) / flowsPerSec;
}

void
FlowGenerator::setTimeLimits(simtime_picosec startTime, 
                             simtime_picosec endTime)
{
    if (_useTrace) {
        EventList::Get().sourceIsPending(*this, _flowTrace.front().first);
    } else {
        EventList::Get().sourceIsPending(*this, startTime);
    }
    EventList::Get().sourceIsPending(*this, endTime);
    _endTime = endTime;
}

void
FlowGenerator::setEndhostQueue(linkspeed_bps qRate, 
                               uint64_t qBuffer)
{
    _endhostQ = true;
    _endhostQrate = qRate;
    _endhostQbuffer = qBuffer;
}

void
FlowGenerator::setReplaceFlow(uint32_t maxFlows, 
                              double offRatio)
{
    _replaceFlow = true;
    _maxFlows = maxFlows;

    double avgFCT = (_workload._avgFlowSize * 8.0)/(_flowRate/_maxFlows);
    _avgFlowArrivalTime = timeFromSec(avgFCT)/_maxFlows;

    _avgOffTime = llround(timeFromSec(avgFCT) * offRatio / (1 + offRatio));
}

void
FlowGenerator::setPrefix(string prefix)
{
    _prefix = prefix;
}

void
FlowGenerator::setTrace(string filename)
{
    _useTrace = true;

    FILE *fp = fopen(filename.c_str(), "r");
    if (fp == NULL) {
        fprintf(stderr, "Error opening trace file: %s\n", filename.c_str());
        exit(1);
    }

    uint32_t fid;
    uint32_t fsize;
    uint64_t fstart;

    /* Assumes the file is sorted by flow arrival. */
    while (fscanf(fp, "flow-%u %lu %*lu %u %*lu %*lu ", &fid, &fstart, &fsize) != EOF) {
        _flowTrace.push_back(make_pair(timeFromUs(fstart), fsize));
    }

    fclose(fp);
}

void
FlowGenerator::doNextEvent()
{
    if (EventList::Get().now() == _endTime) {
        dumpLiveFlows();
        return;
    }

    // Get flowsize from given distriubtion or trace.
    uint64_t flowSize;

    if (_useTrace) {
        flowSize = _flowTrace.front().second;
        _flowTrace.pop_front();
    } else {
        flowSize = _workload.generateFlowSize();
    }

    // Create the flow.
    createFlow(flowSize, 0);
    _concurrentFlows++;

    // Get next flow arrival from given distriubtion or trace.
    simtime_picosec nextFlowArrival;

    if (_useTrace) {
        if (_flowTrace.size() > 0) {
            nextFlowArrival = _flowTrace.front().first - EventList::Get().now();
        } else {
            return;
        }
    } else {
        nextFlowArrival = exponential(1.0/_avgFlowArrivalTime);
    }

    // Schedule next flow.
    if (_replaceFlow == false || _concurrentFlows < _maxFlows) {
        EventList::Get().sourceIsPendingRel(*this, nextFlowArrival);
    }
}

void
FlowGenerator::createFlow(uint64_t flowSize, 
                          simtime_picosec startTime)
{
    // Generate a random route.
    route_t *routeFwd = NULL, *routeRev = NULL;
    uint32_t src_node = 0, dst_node = 0;
    _routeGen(routeFwd, routeRev, src_node, dst_node);

    // Generate next start time adding jitter.
    simtime_picosec start_time = EventList::Get().now() + startTime + llround(drand() * timeFromUs(5));
    simtime_picosec deadline = timeFromSec((flowSize * 8.0) / speedFromGbps(0.8));

    // If flag set, append an endhost queue.
    if (_endhostQ) {
        Queue *endhostQ = new Queue(_endhostQrate, _endhostQbuffer, NULL);
        routeFwd->insert(routeFwd->begin(), endhostQ);
    }

    DataSource *src;
    DataSink *snk;

    switch (_endhost) {
        case DataSource::PKTPAIR:
            src = new PacketPairSrc(NULL, flowSize);
            snk = new PacketPairSink();
            break;

        case DataSource::TIMELY:
            src = new TimelySrc(NULL, flowSize);
            snk = new TimelySink();
            break;

        default: { // TCP variant
                     // TODO: option to supply logtcp.
                     src = new TcpSrc(NULL, NULL, flowSize);
                     snk = new TcpSink();

                     if (_endhost == DataSource::DCTCP || _endhost == DataSource::D_DCTCP) {
                         TcpSrc::_enable_dctcp = true;
                     }

                     if (_endhost == DataSource::D_TCP || _endhost == DataSource::D_DCTCP) {
                         src->_enable_deadline = true;
                     }
                 }
    }

    src->setName(_prefix + "src" + to_string(_flowsGenerated));
    snk->setName(_prefix + "snk" + to_string(_flowsGenerated));
    src->_node_id = src_node;
    snk->_node_id = dst_node;

    src->setDeadline(start_time + deadline);

    routeFwd->push_back(snk);
    routeRev->push_back(src);
    src->connect(start_time, *routeFwd, *routeRev, *snk);
    src->setFlowGenerator(this);

    _liveFlows[src->id] = src;

    _flowsGenerated++;
}

void
FlowGenerator::finishFlow(uint32_t flow_id)
{
    if (_liveFlows.erase(flow_id) == 0) {
        return;
    }

    if (_replaceFlow) {
        uint64_t flowSize = _workload.generateFlowSize();
        uint64_t sleepTime = 0;

        if (_avgOffTime > 0) {
            sleepTime = llround(exponential(1.0L / _avgOffTime));
        }

        createFlow(flowSize, sleepTime);
    }
}

void
FlowGenerator::dumpLiveFlows()
{
    cout << endl << "Live Flows: " << _liveFlows.size() << endl;
    for (auto flow : _liveFlows) {
        DataSource *src = flow.second;
        src->printStatus();
    }

    // TODO: temp hack, remove later.
    /*
       uint64_t cumul = 0;
       cout << "Initial slacks" << endl;
       for (auto it = TcpSrc::slacks.begin(); it != TcpSrc::slacks.end(); it++) {
       cumul += it->second;
       cout << it->first << " " << it->second << " " << (cumul * 100.0) / TcpSrc::totalPkts << endl;
       }

       cumul = 0;
       cout << "Final slacks" << endl;
       for (auto it = TcpSink::slacks.begin(); it != TcpSink::slacks.end(); it++) {
       cumul += it->second;
       cout << it->first << " " << it->second << " " << (cumul * 100.0) / TcpSink::totalPkts << endl;
       }
       */
}
