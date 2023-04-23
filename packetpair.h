#ifndef PACKET_PAIR_H
#define PACKET_PAIR_H

#include "eventlist.h"
#include "datasource.h"

#define PKTPAIR_DELTA timeFromUs(20)
#define PKTPAIR_BETA 0.50L
#define PKTPAIR_GAIN 0.50L
#define RTT_GAIN 0.125L

class PacketPairSink;
class FlowGenerator;

class PacketPairSrc : public DataSource
{
friend class PacketPairSink;
public:
    PacketPairSrc(TrafficLogger *pktlogger, uint64_t flowsize = 0,
            simtime_picosec duration = 0);

    void printStatus();
    void doNextEvent();
    void receivePacket(Packet &pkt);

    // Flow status.
    enum FlowStatus {
        IDLE,
        STARTUP,
        NORMAL,
        RECOVERY,
        FINISH
    } _state;

    // Congestion statistics.
    uint64_t _recover_seq;
    uint16_t _dupacks;
    uint64_t _bdp_estimate;

    // Bottleneck link estimates.
    linkspeed_bps _rate_estimate;
    double _rtt_gradient;
    simtime_picosec _pktpair_ewma;

    // Number of estimated packet drops.
    uint32_t _drops;

    // RTT, RTO estimates.
    simtime_picosec _rtt, _rto, _mdev;
    simtime_picosec _min_rtt;
    simtime_picosec _prev_rtt;
    simtime_picosec _first_rto, _rto_timeout;

    // Periodic RTT measurement variables (bw, ecn, ..)
    simtime_picosec _last_rtt_update;
    uint64_t _last_rtt_bytes;
    linkspeed_bps _measured_rate;

    // ECN variables.
    double _alpha;
    uint32_t _marked_pkts;
    uint32_t _total_pkts;

private:
    void sendPackets(simtime_picosec current_ts);
    void retransmitPacket(simtime_picosec current_ts);
    void transmitPacketPair(simtime_picosec current_ts);
};

class PacketPairSink : public DataSink
{
friend class PacketPairSrc;
public:
    PacketPairSink();
    void receivePacket(Packet &pkt);

    // Packet-pair measurements.
    simtime_picosec _pktpairdiff;
    simtime_picosec _first_pair_ts;
    uint64_t _first_pair_seqno;
};

#endif
