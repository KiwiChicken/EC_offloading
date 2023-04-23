#ifndef TIMELY_H
#define TIMELY_H

#include "eventlist.h"
#include "datasource.h"

#define T_LOW timeFromUs(20)
#define T_HIGH timeFromUs(100)
#define TIMELY_ALPHA 0.02L
#define TIMELY_DELTA 10000000L // 10mbps
#define TIMELY_BETA 0.8L

class TimelySink;
class FlowGenerator;

class TimelySrc : public DataSource
{
friend class TimelySink;
public:
    TimelySrc(TrafficLogger *pktlogger, uint64_t flowsize = 0,
            simtime_picosec duration = 0);

    void printStatus();
    void doNextEvent();
    void receivePacket(Packet &pkt);

    // Flow status.
    enum FlowStatus {
        IDLE,
        NORMAL,
        RECOVERY,
        FINISH
    } _state;

    // Congestion statistics.
    uint64_t _recover_seq;
    uint16_t _dupacks;
    uint64_t _bdp_estimate;
    linkspeed_bps _rate;

    // Number of estimated packet drops.
    uint32_t _drops;

    // RTT, RTO estimates.
    simtime_picosec _rtt, _rto, _mdev;
    simtime_picosec _rto_timeout;
    simtime_picosec _min_rtt;
    simtime_picosec _prev_rtt;
    double _rtt_diff;
    double _rtt_gradient;

    // Periodic RTT measurement variables (bw, ecn, ..)
    simtime_picosec _last_rtt_update;
    uint64_t _last_rtt_bytes;
    linkspeed_bps _measured_rate;

private:
    void sendPackets(simtime_picosec current_ts);
    void retransmitPacket(simtime_picosec current_ts);
};

class TimelySink : public DataSink
{
friend class TimelySrc;
public:
    TimelySink();
    void receivePacket(Packet &pkt);
};

#endif
