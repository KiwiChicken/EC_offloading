/*
 * EC Offload packet header
 */
#ifndef EC_OFFLOAD_H_
#define EC_OFFLOAD_H_

#include "eventlist.h"
#include "datasource.h"
#include "tcp.h"
#include "flow-generator.h"
#include "ec_offload_route.h"

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
        TcpSrc(NULL, pktlogger, flowsize, duration) {
        simtime_picosec current_ts = EventList::Get().now();

        auto src = new EcFromFPGASrc(NULL);
        auto dst = new EcFromFPGASink();

        src->setName("test_src");
        dst->setName("test_dst");
        src->_node_id = 0;
        dst->_node_id = 1;

        auto route_fwd = new route_t();
        route_fwd->push_back(dst);
        auto route_rcv = new route_t();
        route_rcv->push_back(src);

        src->connect(current_ts, *route_fwd, *route_rcv, *dst);

        _src = src;
    }

    void printStatus();
    void doNextEvent();
    void receivePacket(Packet &pkt);

private:
    EcFromFPGASrc* _src;
};

// FPGA
class EcToFPGASink : public TcpSink
{
    friend class EcToFPGASrc;

public:
    EcToFPGASink() : 
        TcpSink(), 
        _pkt_count(0),
        _forwarding()
    {
        // TODO: initialize forwarding table
        simtime_picosec current_ts = EventList::Get().now();

        auto flow_routes = new std::vector<ec_offload::flow_info_t>(); 
        ec_offload::genDstRoutes(*flow_routes);
        // for (auto route : flow_routes) {
        //     EcFromFPGASrc *src = new EcFromFPGASrc(NULL);
        //     EcFromFPGASink *dst = new EcFromFPGASink();

        //     src->setName(str() + "->" + std::to_string(route.dst));
        //     dst->setName(str() + "<-" + std::to_string(route.dst));
        //     src->_node_id = _node_id;
        //     dst->_node_id = route.dst;

        //     route.fwd_route->push_back(dst);
        //     route.rcv_route->push_back(src);
        //     src->connect(current_ts, *route.fwd_route, *route.rcv_route, *dst);
        //     _forwarding.push_back(src);
        // }
        std::cout << "test1" << std::endl << std::flush;
        delete flow_routes;
        std::cout << "test2" << std::endl << std::flush;
    }

    void receivePacket(Packet &pkt);
    void printStatus();

    bool isFinished() {
        for (auto it = _forwarding.begin(); it != _forwarding.end(); it++) {
            if ((*it)->isFinished()) {
                delete *it;
                _forwarding.erase(it);
            } else {
                return false;
            }
        }
        // all forwarding routes have completed
        return true;
    }

private:
    uint64_t _pkt_count;
    std::vector<EcFromFPGASrc*> _forwarding;
};

#endif