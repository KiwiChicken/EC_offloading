/*
 * Loggers header
 */
#ifndef LOGGERS_H
#define LOGGERS_H

#include "eventlist.h"
#include "logfile.h"
#include "loggertypes.h"
#include "network.h"
#include "queue.h"
#include "tcp.h"
#include "prof.h"

#include <vector>

class QueueLoggerSimple : public Logger, public QueueLogger
{
    public:
        void logQueue(Queue& queue, QueueLogger::QueueEvent ev, Packet& pkt)
        {
            _logfile->writeRecord(QueueLogger::QUEUE_EVENT, queue.id, ev,
                    (double)queue._queuesize, pkt.flow().id, pkt.id());
        }
};

class TrafficLoggerSimple : public Logger, public TrafficLogger
{
    public:
        void logTraffic(Packet& pkt, Logged& location, TrafficEvent ev)
        {
            _logfile->writeRecord(TrafficLogger::TRAFFIC_EVENT, location.id,
                    ev,pkt.flow().id, pkt.id(), 0);
        }
};

class TcpLoggerSimple : public Logger, public TcpLogger
{
    public:
        void logTcp(TcpSrc &tcp, TcpEvent ev)
        {
            _logfile->writeRecord(TcpLogger::TCP_EVENT, tcp.id, ev, tcp._cwnd,
                    tcp._highest_sent - tcp._last_acked, tcp._state);

            _logfile->writeRecord(TcpLogger::TCP_STATE, tcp.id, TcpLogger::TCPSTATE_CNTRL,
                    tcp._cwnd, tcp._ssthresh, tcp._recover_seq);

            _logfile->writeRecord(TcpLogger::TCP_STATE, tcp.id, TcpLogger::TCPSTATE_SEQ,
                    tcp._last_acked, tcp._highest_sent, tcp._RFC2988_RTO_timeout);
        }
};

class QueueLoggerSampling : public Logger, public QueueLogger, public EventSource
{
    public:
        QueueLoggerSampling(simtime_picosec period);
        void logQueue(Queue& queue, QueueEvent ev, Packet& pkt);
        void doNextEvent();
    private:
        Queue* _queue;
        simtime_picosec _lastlook;
        simtime_picosec _period;
        mem_b _lastq;
        bool _seenQueueInD;
        mem_b _minQueueInD;
        mem_b _maxQueueInD;
        mem_b _lastDroppedInD;
        mem_b _lastIdledInD;
        int _numIdledInD;
        int _numDropsInD;
        double _cumidle;
        double _cumarr;
        double _cumdrop;
        uint32_t _bytesInD;
        bool _printed;
};

class SinkLoggerSampling : public Logger, public EventSource
{
    public:
        SinkLoggerSampling(simtime_picosec period);
        void doNextEvent();
        void monitorSink(DataSink *sink);
    private:
        std::vector<DataSink *> _sinks;
        std::vector<uint64_t> _last_seq;
        std::vector<double> _last_rate;

        simtime_picosec _last_time;
        simtime_picosec _period;
};

#if MING_PROF
class TcpLoggerSampling : public Logger, public TcpLogger, public EventSource
{
    public:
        TcpLoggerSampling(simtime_picosec period);
        void logTcp(TcpSrc &tcp, TcpEvent ev);
        void doNextEvent();
    private:
        TcpSrc *_tcp;
        simtime_picosec _period;
};
#endif

class AggregateTcpLogger : public Logger, public EventSource
{
    public:
        AggregateTcpLogger(simtime_picosec period);
        void doNextEvent();
        void monitorTcp(TcpSrc& tcp);
    private:
        simtime_picosec _period;
        typedef std::vector<TcpSrc*> tcplist_t;
        tcplist_t _monitoredTcps;
};

#endif /* LOGGERS_H */
