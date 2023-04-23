/*
 * TCP packet header
 */
#ifndef TCP_H_
#define TCP_H_

#include "eventlist.h"
#include "datasource.h"

#define DCTCP_GAIN 0.0625

class TcpSink;
class FlowGenerator;

class TcpSrc : public DataSource
{
    friend class TcpSink;
    public:
    TcpSrc(TcpLogger *logger, TrafficLogger *pktlogger,
            uint32_t flowsize = 0, simtime_picosec duration = 0);

    void printStatus();
    void doNextEvent();
    void receivePacket(Packet &pkt);

    // Flow status.
    enum FlowStatus {
        IDLE,
        SLOW_START,
        CONG_AVOID,
        FAST_RECOV,
        FINISH
    } _state;

    // Congestion statistics (in bytes).
    uint32_t _ssthresh;
    uint32_t _cwnd;
    uint64_t _recover_seq;

    uint16_t _dupacks;
    uint32_t _drops;

    // RTT, RTO estimates.
    simtime_picosec _rtt, _rto, _mdev;
    simtime_picosec _RFC2988_RTO_timeout;

    // DCTCP variables;
    double _alpha;
    uint32_t _marked_pkts;
    uint32_t _total_pkts;
    uint64_t _dctcp_cwnd;

    // DCTCP enable flag.
    static bool _enable_dctcp;

    static std::map<uint64_t, uint64_t> slacks;
    static uint64_t totalPkts;

    private:
    // Mechanism
    void inflateWindow();
    void sendPackets();
    void retransmitPacket(int reason);

    // Housekeeping
    TcpLogger *_logger;
};

class TcpSink : public DataSink
{
    friend class TcpSrc;
    public:
    TcpSink();
    void receivePacket(Packet &pkt);
    void printStatus();

    static std::map<uint64_t, uint64_t> slacks;
    static uint64_t totalPkts;
};

#endif /* TCP_H_ */
