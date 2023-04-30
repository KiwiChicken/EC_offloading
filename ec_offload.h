/*
 * EC Offload packet header
 */
#ifndef EC_OFFLOAD_H_
#define EC_OFFLOAD_H_

#include "eventlist.h"
#include "datasource.h"
#include "tcp.h"
#include "flow-generator.h"
#include "ec_route.h"

// FPGA, event based TCP
class EcFromFPGASrc : public DataSource
{
    friend class EcFromFPGASink;

public:
    EcFromFPGASrc(TrafficLogger *pktlogger, 
                  uint32_t flowsize = 0, 
                  simtime_picosec duration = 0, 
                  int pkt_size = MSS_BYTES) : 
        DataSource(pktlogger, flowsize, duration),                
        _state(IDLE),
        _ssthresh(0xffffffff),
        _cwnd(0),
        _recover_seq(0),
        _dupacks(0),
        _drops(0),
        _rtt(0),
        _rto(timeFromUs(INIT_RTO_US)),
        _mdev(0),
        _RFC2988_RTO_timeout(0),
        _pkt_size(pkt_size),
        _pending_pkts(0),
        _complete(false) {}

    ~EcFromFPGASrc() {
        delete _route_fwd;
        delete _route_rev;
        delete _sink;
    }

    void printStatus();
    void doNextEvent();
    void receivePacket(Packet &pkt);

    void sendOnePacket();

    int _pkt_size;
    int _pending_pkts;
    bool _complete;

    // Flow status.
    enum FlowStatus {
        IDLE,
        SLOW_START,
        CONG_AVOID,
        FAST_RECOV,
        FINISH,
    } _state;

    bool isFinished() {
        return _state == FINISH;
    }

    // Congestion statistics (in bytes).
    uint32_t _ssthresh;
    uint32_t _cwnd;
    uint64_t _recover_seq;

    uint16_t _dupacks;
    uint32_t _drops;

    // RTT, RTO estimates.
    simtime_picosec _rtt, _rto, _mdev;
    simtime_picosec _RFC2988_RTO_timeout;

private:
    // Mechanism
    void inflateWindow();
    void retransmitPacket(int reason);

};

// Dst endhost
class EcFromFPGASink : public DataSink
{
    friend class EcFromFPGASrc;

public:
    EcFromFPGASink() : DataSink() {}

    void receivePacket(Packet &pkt);
    void printStatus();

private:
};

// Src endhost
class EcToFPGASrc : public TcpSrc
{
    friend class EcToFPGASink;

public:
    EcToFPGASrc(TrafficLogger *pktlogger, uint32_t flowsize = 0, simtime_picosec duration = 0) : 
        TcpSrc(NULL, pktlogger, flowsize, duration) {}

    void printStatus();
    void doNextEvent();
    void receivePacket(Packet &pkt);

private:
};

// FPGA
class EcToFPGASink : public TcpSink, public EventSource
{
    friend class EcToFPGASrc;

public:
    EcToFPGASink(uint32_t _node_id, simtime_picosec exec_time = 0, uint64_t mem = 0) : 
        TcpSink(), 
        EventSource("EcToFPGASink"),
        _pkt_count(0),
        _exec_time(exec_time),
        _next_idle(0),
        _mem_limit(mem),
        _mem_usage(0),
        _pending_events(0),
        _highest_received(0),
        _forwarding()
    {
        simtime_picosec current_ts = EventList::Get().now();

        auto flow_routes = std::vector<ec_route::flow_info_t>(); 
        ec_route::genRoutes(_node_id, flow_routes);

        for (auto route : flow_routes) {
            EcFromFPGASrc *src = new EcFromFPGASrc(NULL);
            EcFromFPGASink *dst = new EcFromFPGASink();

            src->setName(DataSink::str() + "->" + std::to_string(route.dst));
            dst->setName(DataSink::str() + "<-" + std::to_string(route.dst));
            src->_node_id = _node_id;
            dst->_node_id = route.dst;

            route.fwd_route->push_back(dst);
            route.rcv_route->push_back(src);
            src->connect(current_ts, *route.fwd_route, *route.rcv_route, *dst);
            _forwarding.push_back(src);
        }
    }

    ~EcToFPGASink() {
        for (auto it = _forwarding.begin(); it != _forwarding.end(); it++)
            delete *it;
    }

    void receivePacket(Packet &pkt);
    void printStatus();

    void doNextEvent();

    bool isFinished() {
        for (auto it = _forwarding.begin(); it != _forwarding.end(); it++) {
            if (!(*it)->isFinished()) 
                return false;
        }
        // all forwarding routes have completed
        return true;
    }

private:
    uint64_t _pending_events;
    simtime_picosec _exec_time;
    simtime_picosec _next_idle;
    uint64_t _mem_limit;
    uint64_t _mem_usage;
    uint64_t _pkt_count;
    uint64_t _highest_received;
    std::vector<EcFromFPGASrc*> _forwarding;

    void broadcastEc();
};

#endif