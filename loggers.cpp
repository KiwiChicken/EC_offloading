/*
 * Loggers
 */
#include "loggers.h"
#include "prof.h"

using namespace std;

QueueLoggerSampling::QueueLoggerSampling(simtime_picosec period)
    : EventSource("QueueLogSampling"),
    _queue(NULL), 
    _lastlook(0), 
    _period(period), 
    _lastq(0), 
    _seenQueueInD(false), 
    _cumidle(0), 
    _cumarr(0), 
    _cumdrop(0), 
    _bytesInD(0), 
    _printed(false)
{	
    EventList::Get().sourceIsPendingRel(*this, 0);
}

void
QueueLoggerSampling::doNextEvent() 
{
    EventList::Get().sourceIsPendingRel(*this, _period);

    if (_queue == NULL) {
        return;
    }

    if (!_seenQueueInD) { // queue size hasn't changed in the past D time units
        _logfile->writeRecord(QUEUE_APPROX, _queue->id, QUEUE_RANGE, (double)_lastq, (double)_lastq, (double)_lastq);
        _logfile->writeRecord(QUEUE_APPROX, _queue->id, QUEUE_OVERFLOW, 0, 0, _bytesInD/timeAsSec(_period));
    }
    else { // queue size has changed
        _logfile->writeRecord(QUEUE_APPROX, _queue->id, QUEUE_RANGE,
                (double)_lastq, (double)_minQueueInD, (double)_maxQueueInD);

        _logfile->writeRecord(QUEUE_APPROX, _queue->id, QUEUE_OVERFLOW,
                -(double)_lastIdledInD, (double)_lastDroppedInD, _bytesInD/timeAsSec(_period));
    }

    _seenQueueInD = false;
    simtime_picosec now = EventList::Get().now();
    simtime_picosec dt_ps = now - _lastlook;
    _lastlook = now;
    _bytesInD = 0;
    _printed = false;

    // if the queue is empty, we've just been idling
    if ((_queue != NULL) && (_queue->_queuesize == 0)) {
        _cumidle += timeAsSec(dt_ps);
    }

    _logfile->writeRecord(QUEUE_RECORD, _queue->id, CUM_TRAFFIC, _cumarr, _cumidle, _cumdrop);

#if MING_PROF
    _queue->printStats();
#endif
}

void
QueueLoggerSampling::logQueue(Queue& queue, 
                              QueueEvent ev, 
                              Packet &pkt)
{
    if (_queue == NULL) {
        _queue = &queue;
    } else {
        assert(&queue == _queue);
    }

    _lastq = queue._queuesize;

    if (!_seenQueueInD) {
        _seenQueueInD = true;
        _minQueueInD = queue._queuesize;
        _maxQueueInD = _minQueueInD;
        _lastDroppedInD = 0;
        _lastIdledInD = 0;
        _numIdledInD = 0;
        _numDropsInD = 0;
    } else {
        _minQueueInD = min(_minQueueInD, queue._queuesize);
        _maxQueueInD = max(_maxQueueInD, queue._queuesize);
    }

    simtime_picosec now = EventList::Get().now();
    simtime_picosec dt_ps = now - _lastlook;
    _lastlook = now;
    double dt = timeAsSec(dt_ps);

    switch (ev) {
        case PKT_SERVICE: // we've just been working
            _bytesInD += pkt.size();
            break;
        case PKT_ENQUEUE:
            _cumarr += timeAsSec(queue.drainTime(&pkt));
            if (queue._queuesize > pkt.size()) {
                // we've just been working
            } else { // we've just been idling 
                mem_b idledwork = queue.serviceCapacity(dt_ps);
                _cumidle += dt; 
                _lastIdledInD = idledwork;
                _numIdledInD++;
            }
            break;
        case PKT_DROP: // assume we've just been working
            assert(queue._queuesize >= pkt.size());
            // it is possible to drop when queue is idling, but this logger can't make sense of it

            double localdroptime = timeAsSec(queue.drainTime(&pkt));
            _cumarr += localdroptime;
            _cumdrop += localdroptime;
            _lastDroppedInD += pkt.size();
            _numDropsInD++;
            break;
    }

    //if (!_printed && queue._queuesize > 50000) {
    //    queue.printStats();
    //    _printed = true;
    //}
}


AggregateTcpLogger::AggregateTcpLogger(simtime_picosec period)
    : EventSource("bunchofflows"), 
    _period(period)
{
    EventList::Get().sourceIsPendingRel(*this, period);
}

void
AggregateTcpLogger::monitorTcp(TcpSrc& tcp)
{
    _monitoredTcps.push_back(&tcp);
}

void
AggregateTcpLogger::doNextEvent()
{
    EventList::Get().sourceIsPendingRel(*this, _period);

    double totunacked = 0;
    double totcwnd = 0;
    int numflows = 0;

    for (tcplist_t::iterator i = _monitoredTcps.begin(); i != _monitoredTcps.end(); i++) {
        TcpSrc* tcp = *i;
        uint32_t cwnd = tcp->_cwnd;
        uint32_t unacked = tcp->_highest_sent - tcp->_last_acked;
        totcwnd += cwnd;
        totunacked += unacked;
        numflows++;
    }

    _logfile->writeRecord(TcpLogger::TCP_RECORD, id, TcpLogger::AVE_CWND,
            totcwnd/numflows, totunacked/numflows, 0);
}


SinkLoggerSampling::SinkLoggerSampling(simtime_picosec period):
    EventSource("SinkSampling"), 
    _period(period)
{
    EventList::Get().sourceIsPendingRel(*this, 0);
}

void 
SinkLoggerSampling::monitorSink(DataSink* sink)
{
    _sinks.push_back(sink);
    _last_seq.push_back(sink->cumulative_ack());
    _last_rate.push_back(0);
}

void 
SinkLoggerSampling::doNextEvent()
{
    EventList::Get().sourceIsPendingRel(*this, _period);

    simtime_picosec now = EventList::Get().now();
    simtime_picosec delta = now - _last_time;
    _last_time = now;

    for (uint64_t i = 0; i < _sinks.size(); i++) {
        // this deals with resets for periodic sources
        if (_last_seq[i] <= _sinks[i]->cumulative_ack()) {
            DataAck::seq_t deltaB = _sinks[i]->cumulative_ack() - _last_seq[i];
            double rate = deltaB / timeAsSec(delta); // In Bps

            _logfile->writeRecord(TcpLogger::TCP_SINK, _sinks[i]->id, TcpLogger::RATE,
                    rate, _sinks[i]->drops(), _sinks[i]->cumulative_ack());

            _last_rate[i] = rate;
        }
        _last_seq[i] = _sinks[i]->cumulative_ack();
    }
}
