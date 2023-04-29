#include "ec_offload.h"

using namespace std;

void EcToFPGASrc::printStatus() {
    // TcpSrc::printStatus();
}

void EcToFPGASrc::doNextEvent() {
    // TcpSrc::doNextEvent();
    _src->sendOnePacket();
    EventList::Get().sourceIsPendingRel(*this, timeFromUs(MIN_RTO_US));
}

void EcToFPGASrc::receivePacket(Packet &pkt) {
    // TcpSrc::receivePacket(pkt);
}

void EcToFPGASink::receivePacket(Packet &pkt) {
    // TODO: send one packet to the corresponding dst
    // TcpSink::receivePacket(pkt);
}

void EcToFPGASink::printStatus() {
    // TODO
}

// =========================================================

void
EcFromFPGASrc::retransmitPacket(int reason)
{
    DataPacket *p = DataPacket::newpkt(_flow, *_route_fwd, _last_acked + 1, _pkt_size);
    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);
    p->set_ts(EventList::Get().now());

    if (_enable_deadline) {
        p->setFlag(Packet::DEADLINE);
        p->setPriority(0);
    }

    _packets_sent += _pkt_size;
    p->sendOn();

    if(_RFC2988_RTO_timeout == 0) { // RFC2988 5.1
        _RFC2988_RTO_timeout = EventList::Get().now() + _rto;
    }
}

void
EcFromFPGASrc::inflateWindow()
{
    // Be very conservative - possibly not the best we can do, but
    // the alternative has bad side effects.
    int newly_acked = (_last_acked + _cwnd) - _highest_sent;
    int increment;

    if (newly_acked < 0) {
        return;
    } else if (newly_acked > _pkt_size) {
        newly_acked = _pkt_size;
    }

    if (_cwnd < _ssthresh) {
        // Slow start phase.
        increment = min(_ssthresh - _cwnd, (uint32_t)newly_acked);
    } else {
        // Congestion avoidance phase.
        increment = (newly_acked * _pkt_size) / _cwnd;
        if (increment == 0) {
            increment = 1;
        }
    }
    _cwnd += increment;
}

void EcFromFPGASrc::sendOnePacket() {
    // TODO: add retry and flow control
    simtime_picosec current_ts = EventList::Get().now();
    
    if (_last_acked + _cwnd < _highest_sent + _pkt_size) {
        _pending_pkts += 1;
        return;
    }

    DataPacket *p = DataPacket::newpkt(_flow, *_route_fwd, _highest_sent + 1, _pkt_size);
    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);
    p->set_ts(current_ts);

    _highest_sent += _pkt_size;
    _packets_sent += _pkt_size;

    std::cout << str() << "Sending packet with seq " << p->seqno() << std::endl;
    p->sendOn();

    if (_RFC2988_RTO_timeout == 0) { // RFC2988 5.1
        _RFC2988_RTO_timeout = current_ts + _rto;
    }
}

void EcFromFPGASrc::printStatus() {
    simtime_picosec current_ts = EventList::Get().now();

    // bytes_transferred/total_bytes == time_elapsed/estimated_fct
    simtime_picosec estimated_fct;
    if (_last_acked > 0) {
        estimated_fct = (_flowsize * (current_ts - _start_time)) / _last_acked;
    } else {
        estimated_fct = 0;
    }

    cout << setprecision(6) << "LiveFlow " << str()
         << " start " << lround(timeAsUs(_start_time))
         << " endBytes " << _last_acked
         << " fct " << timeAsUs(estimated_fct)
         << " sent " << _highest_sent << " " << _packets_sent - _highest_sent
         << " rate " << _last_acked * 8000.0 / (current_ts - _start_time)
         << " cwnd " << _cwnd << endl;
}

void EcFromFPGASrc::doNextEvent() {
    // TODO: setup
    simtime_picosec current_ts = EventList::Get().now();

    // This is a new flow, start sending packets.
    if (_state == IDLE) {
        _state = SLOW_START;
        _cwnd = 2 * MSS_BYTES;
    }

    else if (_complete && _pending_pkts == 0 && _flow._nPackets == 0) {
        _state = FINISH;
        delete _sink;
        delete _route_fwd;
        delete _route_rev;
        return;
    }

    else if (_RFC2988_RTO_timeout != 0 && current_ts >= _RFC2988_RTO_timeout) {

        cout << str() << " at " << timeAsMs(current_ts)
             << " RTO " << timeAsUs(_rto)
             << " MDEV " << timeAsUs(_mdev)
             << " RTT "<< timeAsUs(_rtt)
             << " SEQ " << _last_acked / _pkt_size
             << " CWND "<< _cwnd / _pkt_size
             << " RTO_timeout " << timeAsMs(_RFC2988_RTO_timeout)
             << " STATE " << _state << endl;

        if (_state == FAST_RECOV) {
            uint32_t flightsize = _highest_sent - _last_acked;
            _cwnd = min(_ssthresh, flightsize + _pkt_size);
        }

        _ssthresh = max(_cwnd / 2, (uint32_t)(_pkt_size * 2));

        _cwnd = MSS_BYTES;
        _state = SLOW_START;
        _recover_seq = _highest_sent;
        _highest_sent = _last_acked + _pkt_size;
        _dupacks = 0;

        // Reset rtx timerRFC 2988 5.5 & 5.6
        _rto *= 2;
        _RFC2988_RTO_timeout = current_ts + _rto;

        retransmitPacket(1);
    }

    if (_rtt != 0) {
        EventList::Get().sourceIsPendingRel(*this, _rtt);
    } else {
        EventList::Get().sourceIsPendingRel(*this, timeFromUs(MIN_RTO_US));
    }
}

void EcFromFPGASrc::receivePacket(Packet &pkt) {
    // TODO: bookkeeping for flow control
    simtime_picosec current_ts = EventList::Get().now();
    DataAck *p = (DataAck*)(&pkt);
    uint64_t seqno = p->ackno();
    simtime_picosec ts = p->ts();

    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_RCVDESTROY);
    p->free();

    std::cout << str() << "Receiving ack with seq " << seqno << std::endl;

    if (seqno < _last_acked) {
        cout << "ACK from the past: seqno " << seqno << " _last_acked " << _last_acked << endl;
        return;
    }

    _last_acked = seqno;

    // send next pkt as needed
    while (_pending_pkts > 0 && _last_acked + _cwnd >= _highest_sent + _pkt_size) {
        sendOnePacket();
        _pending_pkts -= 1;
    }
}

void EcFromFPGASink::receivePacket(Packet &pkt) {
    DataPacket *p = (DataPacket*)(&pkt);
    simtime_picosec ts = p->ts();
    uint64_t seqno = p->seqno();
    processDataPacket(*p);  
    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_RCVDESTROY);
    p->free();

    std::cout << str() << "Receiving packet with seq " << seqno << std::endl;

    DataAck *ack = DataAck::newpkt(_src->_flow, *_route, 1, seqno);
    ack->flow().logTraffic(*ack, *this, TrafficLogger::PKT_CREATESEND);
    ack->set_ts(ts);
    if (p->getFlag(Packet::ECN_FWD)) {
        ack->setFlag(Packet::ECN_REV);
    }
    ack->sendOn();
}

void EcFromFPGASink::printStatus() {
    // TODO
}
