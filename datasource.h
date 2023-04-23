/*
 * DataSource header
 */
#ifndef DATASOURCE_H
#define DATASOURCE_H

#include "eventlist.h"
#include "loggertypes.h"
#include "datapacket.h"
#include "datasink.h"

#include <list>

class FlowGenerator;

class DataSource : public EventSource, public PacketSink
{
    public:
        DataSource(TrafficLogger *logger, uint64_t flowsize, simtime_picosec duration);
        virtual ~DataSource() {};

        /* Different types of endhost that implement DataSource */
        enum EndHost {
            TCP,      // TCP New Reno
            DCTCP,    // Datacenter TCP
            D_TCP,    // Deadline TCP
            D_DCTCP,  // Deadline DCTCP
            PKTPAIR,  // Packet pair
            TIMELY
        };

        virtual void printStatus() = 0;
        virtual void doNextEvent() = 0;
        virtual void receivePacket(Packet &pkt) = 0;

        void connect(simtime_picosec start_time, route_t &route_fwd, route_t &route_rev, DataSink &sink);

        void setFlowGenerator(FlowGenerator *flowgen);
        void setDeadline(simtime_picosec deadline);

        uint64_t _flowsize;
        simtime_picosec _duration;
        simtime_picosec _start_time;
        simtime_picosec _deadline;

        uint64_t _packets_sent;
        uint64_t _highest_sent;
        uint64_t _last_acked;

        bool _enable_deadline;

        DataSink *_sink;
        route_t *_route_fwd;
        route_t *_route_rev;

        FlowGenerator *_flowgen;
        PacketFlow _flow;

        uint32_t _node_id;
};

#endif /* DATASOURCE_H */
