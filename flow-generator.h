/*
 * Flow generator header
 */
#ifndef FLOW_GENERATOR_H
#define FLOW_GENERATOR_H

#include "eventlist.h"
#include "loggers.h"
#include "network.h"
#include "datasource.h"
#include "tcp.h"
#include "packetpair.h"
#include "timely.h"
#include "workloads.h"
#include "prof.h"

#include <deque>
#include <functional>

/* Route generator function. */
typedef std::function<void(route_t *&, route_t *&, uint32_t &, uint32_t &)> route_gen_t;

class FlowGenerator : public EventSource
{
    public:
        FlowGenerator(DataSource::EndHost endhost, route_gen_t rg, linkspeed_bps flowRate,
                uint32_t avgFlowSize, Workloads::FlowDist flowSizeDist);
        void doNextEvent();

        /* Set flow generation start time. */
        void setTimeLimits(simtime_picosec startTime, simtime_picosec endTime);

        /* Creates a separate endhost queue for every flow. */
        void setEndhostQueue(linkspeed_bps qRate, uint64_t qBuffer);

        /* Fixes max flows in the systems and replaces them when finished. */
        void setReplaceFlow(uint32_t maxFlows, double offRatio);

        /* Appends a prefix to flow names to differetiate from other generators. */
        void setPrefix(std::string prefix);

        /* Flow arrival using a trace instead of dynamic generation during simulation. */
        void setTrace(std::string filename);

        /* Used by Source to notify the Generator of flow finishing, which can then
         * (optionally) generate a new flow. */
        void finishFlow(uint32_t flow_id);
        void dumpLiveFlows();

    private:
        // Creates a flow in the simulation.
        void createFlow(uint64_t flowSize, simtime_picosec startTime);

        // Returns a flow size according to some distribution.
        uint64_t generateFlowSize();

        std::string _prefix;          // Optional prefix for flows.
        DataSource::EndHost _endhost; // Type of endhost.
        route_gen_t _routeGen;        // Function to generate a route.
        linkspeed_bps _flowRate;      // Target flow rate in bytes/sec.
        uint32_t _flowSizeDist;       // Distribution of flow size [0/1/2] - Uniform/Exp/Pareto.
        uint32_t _flowsGenerated;     // Total number of flow generated.
        simtime_picosec _endTime;     // When to stop generating flows and dump live ones.
        Workloads _workload;          // Type of workload and characteristics.

        // Endhost queue configuration.
        bool _endhostQ;
        linkspeed_bps _endhostQrate;
        uint64_t _endhostQbuffer;

        // Flow replacement configuration.
        bool _useTrace;               // Use a trace for flow generations.
        bool _replaceFlow;            // Replace flows when finished.
        uint32_t _maxFlows;           // Max concurrent flows.
        uint32_t _concurrentFlows;    // Number of concurrent flows.
        simtime_picosec _avgOffTime;  // Sleep duration as fraction of avgFCT.

        // Trace of flow arrivals, if using trace generation. (arrival_time, size)
        std::deque<std::pair<simtime_picosec, uint64_t> > _flowTrace;

        // List of live flows in the system.
        std::unordered_map<uint32_t,DataSource*> _liveFlows;

        // Average flow inter-arrival time, computed using arguments.
        simtime_picosec _avgFlowArrivalTime;

        // Custom flow size distribution.
        std::map<double,uint64_t> _flowSizeCDF;
};

#endif /* FLOW_GENERATOR_H */
