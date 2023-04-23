/*
 * Logger type header
 */
#ifndef LOGGERTYPES_H
#define LOGGERTYPES_H

#include <string>

class Packet;
class TcpSrc;
class Queue;

class Logged
{
public:
    Logged(const std::string &name)
    {
        _name = name;
        id = LASTIDNUM;
        Logged::LASTIDNUM++;
    }

    virtual ~Logged() {}

    void setName(const std::string &name) { _name = name; }
    virtual const std::string& str() { return _name; };

    uint32_t id;
private:
    static uint32_t LASTIDNUM;
    std::string _name;
};

class TrafficLogger
{
public:
    enum EventType { TRAFFIC_EVENT = 3 };
    enum TrafficEvent {
        PKT_ARRIVE     = 0,
        PKT_DEPART     = 1,
        PKT_CREATESEND = 2,
        PKT_DROP       = 3,
        PKT_RCVDESTROY = 4
    };

    virtual void logTraffic(Packet &pkt, Logged &location, TrafficEvent ev) = 0;
    virtual ~TrafficLogger(){};
};

class QueueLogger
{
public:
    enum EventType {
        QUEUE_EVENT  = 0,
        QUEUE_RECORD = 4,
        QUEUE_APPROX = 5
    };
    enum QueueEvent {
        PKT_ENQUEUE = 0,
        PKT_DROP    = 1,
        PKT_SERVICE = 2
    };
    enum QueueRecord { CUM_TRAFFIC = 0 };
    enum QueueApprox {
        QUEUE_RANGE    = 0,
        QUEUE_OVERFLOW = 1
    };

    virtual void logQueue(Queue &queue, QueueEvent ev, Packet &pkt) = 0;
    virtual ~QueueLogger(){};
};

class TcpLogger
{
public:
    enum EventType {
        TCP_EVENT  = 1,
        TCP_STATE  = 2,
        TCP_RECORD = 6,
        TCP_SINK   = 11
    };
    enum TcpEvent {
        TCP_RCV              = 0,
        TCP_RCV_FR_END       = 1,
        TCP_RCV_FR           = 2,
        TCP_RCV_DUP_FR       = 3,
        TCP_RCV_DUP          = 4,
        TCP_RCV_3DUPNOFR     = 5,
        TCP_RCV_DUP_FASTXMIT = 6,
        TCP_TIMEOUT          = 7,
    };
    enum TcpState {
        TCPSTATE_CNTRL = 0,
        TCPSTATE_SEQ   = 1
    };
    enum TcpRecord { AVE_CWND = 0 };
    enum TcpSinkRecord { RATE = 0 };

    virtual void logTcp(TcpSrc &src, TcpEvent ev) = 0;
    virtual ~TcpLogger(){};
};

#endif /* LOGGERTYPES_H */
