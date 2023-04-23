/*
 * TCP 
 */
#include "tcp.h"
#include "flow-generator.h"
#include "prof.h"

#define TRACE_FLOW 0 && "tcp0"

using namespace std;

bool TcpSrc::_enable_dctcp = false;
map<uint64_t, uint64_t> TcpSrc::slacks;
map<uint64_t, uint64_t> TcpSink::slacks;
uint64_t TcpSrc::totalPkts = 0;
uint64_t TcpSink::totalPkts = 0;

TcpSrc::TcpSrc(TcpLogger *logger,
               TrafficLogger *pktlogger,
               uint32_t flowsize,
               simtime_picosec duration)
              :DataSource(pktlogger, flowsize, duration),
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
               _alpha(0.0),
               _marked_pkts(0),
               _total_pkts(0),
               _dctcp_cwnd(0),
               _logger(logger)
{
    // Constructor
}

void
TcpSrc::printStatus()
{
    simtime_picosec current_ts = EventList::Get().now();

    // bytes_transferred/total_bytes == time_elapsed/estimated_fct
    simtime_picosec estimated_fct;
    if (_last_acked > 0) {
        estimated_fct = (_flowsize * (current_ts - _start_time)) / _last_acked;
    } else {
        estimated_fct = 0;
    }

    cout << setprecision(6) << "LiveFlow " << str() << " size " << _flowsize
         << " start " << lround(timeAsUs(_start_time))
         << " endBytes " << _last_acked
         << " fct " << timeAsUs(estimated_fct)
         << " sent " << _highest_sent << " " << _packets_sent - _highest_sent
         << " rate " << _last_acked * 8000.0 / (current_ts - _start_time)
         << " cwnd " << _cwnd
         << " alpha " << _alpha << endl;
}

void
TcpSrc::doNextEvent()
{
    simtime_picosec current_ts = EventList::Get().now();

    // This is a new flow, start sending packets.
    if (_state == IDLE) {
        _state = SLOW_START;
        _cwnd = 2 * MSS_BYTES;
        _dctcp_cwnd = _cwnd;
        sendPackets();
    }

    // Cleanup the finished flow.
    else if (_state == FINISH) {
        // If no more flow packets in the system, delete all objects.
        // Make sure no one else has access to these.
        if (_flow._nPackets == 0) {
            delete _sink;
            delete _route_fwd;
            delete _route_rev;
            delete this;
            return;
        }
    }

    // Retransmission timeout.
    else if (_RFC2988_RTO_timeout != 0 && current_ts >= _RFC2988_RTO_timeout) {

        cout << str() << " at " << timeAsMs(current_ts)
             << " RTO " << timeAsUs(_rto)
             << " MDEV " << timeAsUs(_mdev)
             << " RTT "<< timeAsUs(_rtt)
             << " SEQ " << _last_acked / MSS_BYTES
             << " CWND "<< _cwnd / MSS_BYTES
             << " RTO_timeout " << timeAsMs(_RFC2988_RTO_timeout)
             << " STATE " << _state << endl;

        if (_logger) _logger->logTcp(*this, TcpLogger::TCP_TIMEOUT);

        if (_state == FAST_RECOV) {
            uint32_t flightsize = _highest_sent - _last_acked;
            _cwnd = min(_ssthresh, flightsize + MSS_BYTES);
        }

        _ssthresh = max(_cwnd / 2, (uint32_t)(MSS_BYTES * 2));

        _cwnd = MSS_BYTES;
        _state = SLOW_START;
        _recover_seq = _highest_sent;
        _highest_sent = _last_acked + MSS_BYTES;
        _dupacks = 0;

        // Reset rtx timerRFC 2988 5.5 & 5.6
        _rto *= 2;
        _RFC2988_RTO_timeout = current_ts + _rto;

        retransmitPacket(1);
    }

    // Schedule periodic RTT checks.
    if (_rtt != 0) {
        EventList::Get().sourceIsPendingRel(*this, _rtt);
    } else {
        EventList::Get().sourceIsPendingRel(*this, timeFromUs(MIN_RTO_US));
    }
}

void
TcpSrc::receivePacket(Packet &pkt)
{
    simtime_picosec current_ts = EventList::Get().now();
    DataAck *p = (DataAck*)(&pkt);
    DataAck::seq_t seqno = p->ackno();
    simtime_picosec ts = p->ts();

    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_RCVDESTROY);
    p->free();

    if (_state == FINISH) {
        return;
    }

    if ((_flowsize > 0 && seqno >= _flowsize) ||
            (_duration > 0 && current_ts > _start_time + _duration)) {

        if (_flowgen != NULL) {
            _flowgen->finishFlow(id);
        }
        _state = FINISH;

        // Ming added _flowsize
        cout << setprecision(6) << "Flow " << str() << " " << id << " size " << _flowsize
             << " start " << lround(timeAsUs(_start_time)) << " end " << lround(timeAsUs(current_ts))
             << " fct " << timeAsUs(current_ts - _start_time)
             << " sent " << _highest_sent << " " << _packets_sent - _highest_sent
             << " tput " << _flowsize * 8000.0 / (current_ts - _start_time)
             << " rtt " << timeAsUs(_rtt)
             << " cwnd " << _cwnd
             << " alpha " << _alpha << endl;

        return;
    }

    // Delayed / reordered ack. Shouldn't happen for simple queues.
    if (seqno < _last_acked) {
        cout << "ACK from the past: seqno " << seqno << " _last_acked " << _last_acked << endl;
        return;
    }

    if (TRACE_FLOW == str()) {
        cout << str() << " RECV " << EventList::Get().now() << " " << seqno << endl;
    }

    // Update rtt and update _rto.
    uint64_t m = current_ts - ts;

    if (m > 0) {
        if (_rtt > 0) {
            uint64_t abs;
            if (m > _rtt)
                abs = m - _rtt;
            else
                abs = _rtt - m;

            _mdev = _mdev * 3 / 4 + abs / 4;
            _rtt = _rtt * 7 / 8 + m / 8;
        }
        else {
            _rtt = m;
            _mdev = m/2;
        }
        _rto = _rtt + 4 * _mdev;
        if (_rto < timeFromUs(MIN_RTO_US)) {
            _rto = timeFromUs(MIN_RTO_US);
        }
    }

    if (_enable_dctcp) {
        // Update ECN counters.
        if (pkt.getFlag(Packet::ECN_REV)) {
            _marked_pkts += 1;

            // If in slow_start, exit and update _sshthresh.
            if (_state == SLOW_START && _ssthresh > _cwnd) {
                _state = CONG_AVOID;
                _ssthresh = _cwnd;
            }
        }
        _total_pkts += 1;

        // Update _alpha and _cwnd, roughly once per cwnd of data.
        if (_total_pkts * MSS_BYTES > _dctcp_cwnd) {
            double fractionMarked = ((double)_marked_pkts / _total_pkts);
            _alpha = _alpha * (1 - DCTCP_GAIN) + fractionMarked * DCTCP_GAIN;
            _marked_pkts = 0;
            _total_pkts = 0;

            if (_alpha > 0) {
                _cwnd = _cwnd * (1 - _alpha / 2);
                if (_cwnd < MSS_BYTES) {
                    _cwnd = MSS_BYTES;
                }
                _ssthresh = _cwnd;
            }

            if (_state != FAST_RECOV) {
                _dctcp_cwnd = _cwnd;
            } else {
                _dctcp_cwnd = _ssthresh;
            }
        }
    }

    // Brand new ack.
    if (seqno > _last_acked) {

        // RFC 2988 5.3
        _RFC2988_RTO_timeout = current_ts + _rto;

        // RFC 2988 5.2
        if (seqno == _highest_sent) {
            _RFC2988_RTO_timeout = 0;
        }

        // Best behaviour: proper ack of a new packet, when we were expecting it.
        if (_state != FAST_RECOV) { // _state == SLOW_START || CONG_AVOID
            _last_acked = seqno;
            _dupacks = 0;
            inflateWindow();

            if (_logger) _logger->logTcp(*this, TcpLogger::TCP_RCV);
            sendPackets();
            return;
        }

        // We're in fast recovery, i.e. one packet has been
        // dropped but we're pretending it's not serious.
        if (seqno >= _recover_seq) {
            // got ACKs for all the "recovery window": resume normal service
            uint32_t flightsize = _highest_sent - seqno;
            _cwnd = min(_ssthresh, flightsize + MSS_BYTES);
            _last_acked = seqno;
            _dupacks = 0;
            _state = CONG_AVOID;

            if (_logger) _logger->logTcp(*this, TcpLogger::TCP_RCV_FR_END);
            sendPackets();
            return;
        }

        // In fast recovery, and still getting ACKs for the "recovery window".
        // This is dangerous. It means that several packets got lost, not just
        // the one that triggered FR.
        uint32_t new_data = seqno - _last_acked;
        _last_acked = seqno;

        if (new_data < _cwnd) {
            _cwnd -= new_data;
        } else {
            _cwnd = 0;
        }

        _cwnd += MSS_BYTES;

        if (_logger) _logger->logTcp(*this, TcpLogger::TCP_RCV_FR);
        retransmitPacket(2);
        sendPackets();
        return;
    }

    // It's a dup ack.
    if (_state == FAST_RECOV) {
        // Still in fast recovery; hopefully the prodigal ACK is on it's way.
        _cwnd += MSS_BYTES;

        if (_logger) _logger->logTcp(*this, TcpLogger::TCP_RCV_DUP_FR);
        sendPackets();
        return;
    }

    // Not yet in fast recovery. Wait for more dupacks.
    _dupacks++;

    if (_dupacks != 3) {
        if (_logger) _logger->logTcp(*this, TcpLogger::TCP_RCV_DUP);
        sendPackets();
        return;
    }

    // _dupacks == 3
    if (_last_acked < _recover_seq) {
        // See RFC 3782: if we haven't recovered from timeouts etc. don't do fast recovery.
        if (_logger) _logger->logTcp(*this, TcpLogger::TCP_RCV_3DUPNOFR);
        return;
    }

    // Begin fast retransmit/recovery. (count drops only in CA state)
    _drops++;

    _ssthresh = max(_cwnd / 2, (uint32_t)(MSS_BYTES * 2));
    _cwnd = _ssthresh + 3 * MSS_BYTES;
    _state = FAST_RECOV;

    // _recover_seq is the value of the ack that tells us things are back to normal
    _recover_seq = _highest_sent;

    retransmitPacket(3);
    if (_logger) _logger->logTcp(*this, TcpLogger::TCP_RCV_DUP_FASTXMIT);
}

void
TcpSrc::inflateWindow()
{
    // Be very conservative - possibly not the best we can do, but
    // the alternative has bad side effects.
    int newly_acked = (_last_acked + _cwnd) - _highest_sent;
    int increment;

    if (newly_acked < 0) {
        return;
    } else if (newly_acked > MSS_BYTES) {
        newly_acked = MSS_BYTES;
    }

    if (_cwnd < _ssthresh) {
        // Slow start phase.
        increment = min(_ssthresh - _cwnd, (uint32_t)newly_acked);
    } else {
        // Congestion avoidance phase.
        increment = (newly_acked * MSS_BYTES) / _cwnd;
        if (increment == 0) {
            increment = 1;
        }
    }
    _cwnd += increment;
}

void
TcpSrc::sendPackets()
{
    simtime_picosec current_ts = EventList::Get().now();

    // Already sent out enough bytes.
    if (_flowsize > 0 && _highest_sent >= _flowsize) {
        return;
    }

    if (TRACE_FLOW == str()) {
        cout << str() << " SEND " << current_ts << " " << _highest_sent << " " << _cwnd << endl;
    }

    while (_last_acked + _cwnd >= _highest_sent + MSS_BYTES) {
        DataPacket *p = DataPacket::newpkt(_flow, *_route_fwd, _highest_sent + 1, MSS_BYTES);

        p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);
        p->set_ts(current_ts);

        // pFabric priority.
        //if (ENABLE_PFABRIC) {
        //    p->setPriority(_flowsize);
        //}

        if (_enable_deadline) {
            // Calculate and set deadline for this packet.
            simtime_picosec timeRemaining;
            //if (_deadline - timeFromUs(8) > current_ts) {
            if (_deadline > current_ts) {
                timeRemaining = _deadline - current_ts;
            } else {
                timeRemaining = 0;
            }

            uint64_t packetsRemaining = (_flowsize - _highest_sent) / MSS_BYTES + 1;
            uint64_t slack = (timeRemaining / packetsRemaining);
            uint64_t hist = slack/1000000;

            if (slacks.find(hist) == slacks.end()) {
                slacks[hist] = 1;
            } else {
                slacks[hist] += 1;
            }
            totalPkts += 1;

            p->setFlag(Packet::DEADLINE);
            p->setPriority(llround(timeAsNs(slack)));
        }

        _highest_sent += MSS_BYTES;
        _packets_sent += MSS_BYTES;
        p->sendOn();

        if (_RFC2988_RTO_timeout == 0) { // RFC2988 5.1
            _RFC2988_RTO_timeout = current_ts + _rto;
        }

        if (_flowsize > 0 && _highest_sent >= _flowsize) {
            break;
        }
    }
}

void
TcpSrc::retransmitPacket(int reason)
{
    if (TRACE_FLOW == str()) {
        cout << str() << " RETX " << EventList::Get().now() << " " << reason << endl;
    }

    DataPacket *p = DataPacket::newpkt(_flow, *_route_fwd, _last_acked + 1, MSS_BYTES);
    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);
    p->set_ts(EventList::Get().now());

    // pFabric priority.
    //if (ENABLE_PFABRIC) {
    //    p->setPriority(_flowsize);
    //}

    if (_enable_deadline) {
        p->setFlag(Packet::DEADLINE);
        p->setPriority(0);
    }

    _packets_sent += MSS_BYTES;
    p->sendOn();

    if(_RFC2988_RTO_timeout == 0) { // RFC2988 5.1
        _RFC2988_RTO_timeout = EventList::Get().now() + _rto;
    }
}


TcpSink::TcpSink() : DataSink() {}

void
TcpSink::receivePacket(Packet &pkt)
{
    DataPacket *p = (DataPacket*)(&pkt);
    simtime_picosec ts = p->ts();
    processDataPacket(*p);

    if (p->getFlag(Packet::DEADLINE)) {
        uint64_t hist = p->getPriority()/1000;

        if (slacks.find(hist) == slacks.end()) {
            slacks[hist] = 1;
        } else {
            slacks[hist] += 1;
        }
        totalPkts += 1;
    }

    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_RCVDESTROY);
    p->free();

    DataAck *ack = DataAck::newpkt(_src->_flow, *_route, 1, _cumulative_ack);
    ack->flow().logTraffic(*ack, *this, TrafficLogger::PKT_CREATESEND);
    ack->set_ts(ts);
    if (p->getFlag(Packet::ECN_FWD)) {
        ack->setFlag(Packet::ECN_REV);
    }
    ack->sendOn();
}

