#include "timely.h"
#include "flow-generator.h"

#define TRACE_FLOW 0 && "timelySrc27"

using namespace std;

TimelySrc::TimelySrc(TrafficLogger *pktlogger,
        uint64_t flowsize, simtime_picosec duration)
    : DataSource(pktlogger, flowsize, duration),
      _state(IDLE),
      _recover_seq(0),
      _dupacks(0),
      _bdp_estimate(0),
      _rate(10000000000),
      _drops(0),
      _rtt(0),
      _rto(timeFromUs(INIT_RTO_US)),
      _mdev(0),
      _rto_timeout(0),
      _min_rtt(ULLONG_MAX),
      _prev_rtt(0),
      _rtt_diff(0.0),
      _rtt_gradient(0.0),
      _last_rtt_update(0),
      _last_rtt_bytes(0),
      _measured_rate(0)
{
    // Constructor
}

void
TimelySrc::printStatus()
{
    simtime_picosec current_ts = EventList::Get().now();

    // bytes_transferred/total_bytes == time_elapsed/estimated_fct
    simtime_picosec estimated_fct;
    if (_last_acked > 0) {
        estimated_fct = _flowsize * (current_ts - _start_time) / _last_acked;
    } else {
        estimated_fct = 0;
    }

    cout << setprecision(6) << "LiveFlow " << str() << " size " << _flowsize
         << " start " << lround(timeAsUs(_start_time)) << " end " << _last_acked
         << " fct " << timeAsUs(estimated_fct)
         << " sent " << _highest_sent << " " << _packets_sent - _highest_sent
         << " rate " << _last_acked * 8000.0 / (current_ts - _start_time) << endl;
}

void
TimelySrc::doNextEvent()
{
    simtime_picosec current_ts = EventList::Get().now();

    if (TRACE_FLOW == str()) {
        cout << str() << " EV " << timeAsUs(current_ts) << " " << _state << " "
             << timeAsUs(_rto_timeout) << " " << _flow._nPackets << endl;
    }

    // If this is the first transmission, send a packet-pair, set rto and return.
    if (_state == IDLE) {
        _highest_sent = 0;
        _last_acked = 0;
        _bdp_estimate = 8 * MSS_BYTES;
        _last_rtt_update = current_ts;
        _state = NORMAL;
    }

    // Cleanup the finished flow.
    else if (_state == FINISH) {
        if (_flow._nPackets == 0) {
            delete _sink;
            delete _route_fwd;
            delete _route_rev;
            delete this;
            return;
        }
    }

    // Retransmission timeout.
    else if (_rto_timeout != 0 && current_ts >= _rto_timeout) {

        cout << str() << " at " << timeAsMs(current_ts)
             << " RTO " << timeAsUs(_rto)
             << " MDEV " << timeAsUs(_mdev)
             << " RTT "<< timeAsUs(_rtt)
             << " SEQ " << _last_acked
             << " RTO_timeout " << timeAsMs(_rto_timeout)
             << " STATE " << _state << endl;

        _recover_seq = _highest_sent;
        _highest_sent = _last_acked + MSS_BYTES;
        _dupacks = 0;
        _state = NORMAL;

        _rto *= 2;
        _rto_timeout = current_ts + _rto;

        retransmitPacket(current_ts);
    }

    if (_state != FINISH) {
        sendPackets(current_ts);
    } 

    /* Schedule next transmission. Time to transmit MSS_BYTES at estimated link rate. */
    simtime_picosec nextTransmission = timeFromSec((MSS_BYTES * 8.0)/_rate);

    EventList::Get().sourceIsPendingRel(*this, nextTransmission);
}

void
TimelySrc::receivePacket(Packet& pkt)
{
    simtime_picosec current_ts = EventList::Get().now();
    DataAck *p = (DataAck*)(&pkt);
    DataAck::seq_t seqno = p->ackno(); // What sorcery is this naming?
    //simtime_picosec delay = p->seqno();
    simtime_picosec ts = p->ts();

    pkt.flow().logTraffic(pkt,*this, TrafficLogger::PKT_RCVDESTROY);
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

        cout << setprecision(6) << "Flow " << str() << "-" << id << " size " << _flowsize
             << " start " << lround(timeAsUs(_start_time)) << " end " << lround(timeAsUs(current_ts))
             << " fct " << timeAsUs(current_ts - _start_time)
             << " sent " << _highest_sent << " " << _packets_sent - _highest_sent
             << " rate " << _flowsize * 8000.0 / (current_ts - _start_time)
             << " bdp " << _bdp_estimate
             << " minrtt " << _min_rtt << endl;

        return;
    }

    // Delayed / reordered ack. Shouldn't happen for simple queues.
    if (seqno < _last_acked) {
        cout << str() << " ACK from the past: seqno " << seqno << " _last_acked " << _last_acked << endl;
        return;
    }

    // Update rtt and rto estimates.
    simtime_picosec new_rtt = current_ts - ts;

    if (new_rtt < _min_rtt) {
        _min_rtt = new_rtt;
    }

    if (new_rtt != 0) {
        if (_rtt > 0) {
            uint64_t diff = (new_rtt > _rtt) ? (new_rtt - _rtt) : (_rtt - new_rtt);
            _mdev = 3 * _mdev/4 + diff/4;
            _rtt = 7 * _rtt/8 + new_rtt/8;
        } else {
            _rtt = new_rtt;
            _prev_rtt = new_rtt;
            _mdev = new_rtt/2;
        }

        _rto = _rtt + 4 * _mdev;
        if (_rto < timeFromUs(MIN_RTO_US)) {
            _rto = timeFromUs(MIN_RTO_US);
        }
    }

    // Update rtt gradient measurement.
    if (new_rtt > _prev_rtt) {
        _rtt_diff = _rtt_diff * (1 - TIMELY_ALPHA) + (new_rtt - _prev_rtt) * TIMELY_ALPHA;
    } else {
        _rtt_diff = _rtt_diff * (1 - TIMELY_ALPHA) - (_prev_rtt - new_rtt) * TIMELY_ALPHA;
    }
    _prev_rtt = new_rtt;
    _rtt_gradient = _rtt_diff / _min_rtt;

    // If one RTT has passed, measure transfer rate and update sending rate.
    if (_last_acked > _last_rtt_bytes + _bdp_estimate) {
    //if (_rtt > 0 && current_ts > _last_rtt_update + _rtt) {

        double new_rate;

        if (new_rtt < T_LOW) {
            new_rate = _rate + TIMELY_DELTA;
        } else if (new_rtt > T_HIGH) {
            new_rate = _rate * (1 - TIMELY_BETA * (1 - T_HIGH / new_rtt));
        } else {
            // Patched TIMELY.
            double weight = 2 * _rtt_gradient + 0.5;
            if (weight < 0) {
                weight = 0;
            } else if (weight > 1) {
                weight = 1;
            }

            double error = (new_rtt - T_LOW) / (T_LOW * 1.0L);

            new_rate = (1 - weight) * TIMELY_DELTA + _rate * (1 - 0.008L * weight * error);
        }

        _rate = llround(new_rate);

        if (new_rate > 10000000000) {
            _rate = 10000000000;
        }

        if (new_rate < _rate / 2) {
            _rate = _rate / 2;
        }

        _bdp_estimate = llround(_rate * timeAsSec(_min_rtt) / 8);

        // Measure flow transfer rate.
        _measured_rate = (_last_acked - _last_rtt_bytes) / timeAsSec(current_ts - _last_rtt_update);
        _last_rtt_update = current_ts;
        _last_rtt_bytes = _last_acked;
    }

    if (TRACE_FLOW == str()) {
        cout << str() << " RECV " << timeAsMs(current_ts) << " " << seqno << " " << timeAsUs(_rtt_gradient)
             << " rtt/min/diff: " << timeAsUs(_rtt) << " " << timeAsUs(_min_rtt) << " " << _rtt_gradient
             << " rate: " << _rate << " bdp " << _bdp_estimate << " " << timeAsUs(new_rtt) << endl;
    }


    /* ACK processing */
    // A brand new ack.
    if (seqno > _last_acked) {
        uint64_t bytes_acked = seqno - _last_acked;
        _last_acked = seqno;

        _rto_timeout = current_ts + _rto;
        if (seqno == _highest_sent) {
            _rto_timeout = 0;
        }

        // Best behavior: new ack when we were expecting it.
        if (_state != RECOVERY) {
            _dupacks = 0;
            return;
        }

        // We are in fast recovery.
        if (seqno < _recover_seq) {
            // Probably dropped multiple packets.
            _dupacks = _dupacks - bytes_acked/MSS_BYTES + 1;
            retransmitPacket(current_ts);
        } else {
            // Resume nomal service.
            if (TRACE_FLOW == str()) {
                cout << str() << " at " << timeAsMs(current_ts) << " exiting FR "
                    << _recover_seq << " " << seqno << endl;
            }
            _dupacks = 0;
            _state = NORMAL;
        }

        return;
    }

    // (seqno == _last_acked) It's a dup ack.
    if (_state == RECOVERY) {
        // If already in recovery, keep transmitting. Hopefully we'll get back
        // the missing ack.
        _dupacks++;
        return;
    }

    // Not yet in fast recovery, wait for more dupacks.
    _dupacks++;

    // If we haven't recovered from previous losses, don't do fast recovery.
    if (_dupacks != 3 || _last_acked < _recover_seq) {
        return;
    }

    // There has been a drop(s). Fast retransmit.
    _drops++;

    _state = RECOVERY;
    _recover_seq = _highest_sent;
    retransmitPacket(current_ts);

    if (TRACE_FLOW == str()) {
        cout << str() << " FASTXMIT " << timeAsMs(current_ts) << " entering FR "
             << _recover_seq << " " << seqno << endl;
    }
}

void
TimelySrc::sendPackets(simtime_picosec current_ts)
{
    // Don't send more packets it we have already sent _flowsize bytes.
    if (_flowsize != 0 && _highest_sent >= _flowsize) {
        return;
    }

    // Don't send more packets if we have more than BDP bytes in flight.
    if (_bdp_estimate != 0 && _highest_sent - _last_acked >= (_bdp_estimate + _bdp_estimate / 2 + _dupacks * MSS_BYTES)) {
        return;
    }

    if (TRACE_FLOW == str()) {
        cout << str() << " SEND: " << timeAsMs(current_ts) << " " << _highest_sent
             << " " << _last_acked << " " << (_highest_sent - _last_acked) << endl;
    }

    DataPacket *p;
    p = DataPacket::newpkt(_flow, *_route_fwd, _highest_sent + 1, MSS_BYTES);
    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);
    p->set_ts(current_ts);
    p->sendOn();

    _highest_sent += MSS_BYTES;
    _packets_sent += MSS_BYTES;

    if (_rto_timeout == 0) {
        _rto_timeout = current_ts + _rto;
    }
}

void
TimelySrc::retransmitPacket(simtime_picosec current_ts)
{
    if (TRACE_FLOW == str()) {
        cout << str() << " RETX: " << timeAsMs(current_ts) << " " << _last_acked << endl;
    }

    DataPacket *p;
    p = DataPacket::newpkt(_flow, *_route_fwd, _last_acked + 1, MSS_BYTES);
    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);
    p->set_ts(current_ts);

    if (_last_acked == _highest_sent) {
        _highest_sent += MSS_BYTES;
    }

    _packets_sent += MSS_BYTES;
    p->sendOn();

    if (_rto_timeout == 0) {
        _rto_timeout = current_ts + _rto;
    }
}


TimelySink::TimelySink() : DataSink() {}

void
TimelySink::receivePacket(Packet& pkt)
{
    DataPacket *p = (DataPacket*)(&pkt);
    DataPacket::seq_t seqno = p->seqno();
    simtime_picosec ts = p->ts();
    processDataPacket(*p);

    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_RCVDESTROY);
    p->free();

    if (TRACE_FLOW == _src->str()) {
        cout << str() << " SINK-TS: " << timeAsMs(EventList::Get().now()) << " at " << seqno << endl;
    }

    DataAck *ack = DataAck::newpkt(_src->_flow, *_route, 1, _cumulative_ack);
    ack->flow().logTraffic(*ack, *this, TrafficLogger::PKT_CREATESEND);
    ack->set_ts(ts);
    ack->sendOn();
}

