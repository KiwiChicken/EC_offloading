#include "packetpair.h"
#include "flow-generator.h"

#define TRACE_FLOW 0 && "ppSrc"

using namespace std;

PacketPairSrc::PacketPairSrc(TrafficLogger *pktlogger,
        uint64_t flowsize, simtime_picosec duration)
    : DataSource(pktlogger, flowsize, duration),
      _state(IDLE),
      _recover_seq(0),
      _dupacks(0),
      _bdp_estimate(0),
      _rate_estimate(0),
      _rtt_gradient(0),
      _pktpair_ewma(0),
      _drops(0),
      _rtt(0),
      _rto(timeFromUs(INIT_RTO_US)),
      _mdev(0),
      _min_rtt(ULLONG_MAX),
      _prev_rtt(0),
      _first_rto(0),
      _rto_timeout(0),
      _last_rtt_update(0),
      _last_rtt_bytes(0),
      _measured_rate(0),
      _alpha(0.0),
      _marked_pkts(0),
      _total_pkts(0)
{
    // Constructor
}

void
PacketPairSrc::printStatus()
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
         << " bdp " << _bdp_estimate
         << " sent " << _highest_sent << " " << _packets_sent - _highest_sent
         << " rate " << _last_acked * 8000.0 / (current_ts - _start_time) << endl;
}

void
PacketPairSrc::doNextEvent()
{
    simtime_picosec current_ts = EventList::Get().now();

    if (TRACE_FLOW == str()) {
        printf("%s %.3lf EV %u %.3lf %.3lf\n",
                str().c_str(), timeAsMs(current_ts), _state,
                timeAsUs(_first_rto), timeAsUs(_rto_timeout));
    }

    // If this is the first transmission, send a packet-pair, set rto and return.
    if (_state == IDLE || (_state == STARTUP && current_ts == _first_rto)) {
        _highest_sent = 0;
        _last_acked = 0;
        _bdp_estimate = 2 * MSS_BYTES;
        transmitPacketPair(current_ts);
        _first_rto = current_ts + _rto;
        _last_rtt_update = current_ts;
        _state = STARTUP;
        EventList::Get().sourceIsPendingRel(*this, _rto);
        return;
    }

    // Cleanup the finished flow.
    else if (_state == FINISH) {
        if (_flow._nPackets == 0 && current_ts > _first_rto) {
            delete _sink;
            delete _route_fwd;
            delete _route_rev;
            delete this;
            return;
        }
    }

    // Retransmission timeout.
    else if (_rto_timeout != 0 && current_ts >= _rto_timeout) {

        cout << str() << " TMOUT " << timeAsMs(current_ts)
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

    if (current_ts == _first_rto) {
        return;
    }

    if (_state != FINISH) {
        sendPackets(current_ts);
    } else {
        // Keep scheduling till all packets have been drained.
        EventList::Get().sourceIsPendingRel(*this, _rtt);
    }
}

void
PacketPairSrc::receivePacket(Packet& pkt)
{
    simtime_picosec current_ts = EventList::Get().now();
    DataAck *p = (DataAck*)(&pkt);
    DataAck::seq_t seqno = p->ackno(); // What sorcery is this naming?
    simtime_picosec pktpairdiff = p->seqno(); // Yes, this is horrible!
    simtime_picosec ts = p->ts();

    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);
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

        cout << setprecision(6) << "Flow " << str() << " size " << _flowsize
             << " start " << lround(timeAsUs(_start_time)) << " end " << lround(timeAsUs(current_ts))
             << " fct " << timeAsUs(current_ts - _start_time)
             << " sent " << _highest_sent << " " << _packets_sent - _highest_sent
             << " tput " << speedAsGbps(seqno * 8 / timeAsSec(current_ts - _start_time))
             << " rate " << speedAsGbps(_rate_estimate)
             << " bdp " << _bdp_estimate
             << " alpha " << _alpha
             << " minrtt " << _min_rtt << endl;

        return;
    }

    // Delayed / reordered ack. Shouldn't happen for simple queues.
    if (seqno < _last_acked) {
        cout << str() << " ACK from the past: seqno " << seqno << " _last_acked " << _last_acked << endl;
        return;
    }

    // If a BDP window of data has passed, calculate some statistics.
    if (_rtt > 0 && current_ts > _last_rtt_update + _rtt) {

        // Measure flow transfer rate.
        _measured_rate = ((_last_acked - _last_rtt_bytes) * 8) / timeAsSec(current_ts - _last_rtt_update);
        _last_rtt_update = current_ts;
        _last_rtt_bytes = _last_acked;

        if (TRACE_FLOW == str()) {
            printf("%s %.3lf BW %.3lf %3lf %lu %.3lf\n",
                    str().c_str(), timeAsMs(current_ts),
                    speedAsGbps(_rate_estimate), speedAsGbps(_measured_rate),
                    _bdp_estimate, timeAsUs(_pktpair_ewma));
        }
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
            _mdev = new_rtt/2;
        }

        _rto = _rtt + 4 * _mdev;
        if (_rto < timeFromUs(MIN_RTO_US)) {
            _rto = timeFromUs(MIN_RTO_US);
        }
    }

    // ECN handling.
    if (pkt.getFlag(Packet::ECN_REV)) {
        _marked_pkts += 1;
    }
    _total_pkts += 1;

    if (_total_pkts * MSS_BYTES > _bdp_estimate) {
        double fractionMarked = ((double)_marked_pkts / _total_pkts);
        _alpha = _alpha * (1 - DCTCP_GAIN) + fractionMarked * DCTCP_GAIN;
        _marked_pkts = 0;
        _total_pkts = 0;
    }

    if (pktpairdiff != 0) {
        // If this was the second of the pair packets, store the pair-diff.
        // Time difference should be equal to time to transmit an MSS_BYTES size packet.

        _pktpair_ewma = llround(_pktpair_ewma * (1 - PKTPAIR_GAIN) + pktpairdiff * PKTPAIR_GAIN);

        if (_pktpair_ewma != 0) {
            _rate_estimate = (MSS_BYTES * 8 * timeFromSec(1)) / _pktpair_ewma;

            _bdp_estimate = _rate_estimate * timeAsSec(_min_rtt) / 8;
            if (_bdp_estimate < 2 * MSS_BYTES) {
                _bdp_estimate = 2 * MSS_BYTES;
            }

            // Adjust rate according to most recent rtt above min_rtt.
            //simtime_picosec rtt_diff = _prev_rtt - _min_rtt;

            //if (rtt_diff > PKTPAIR_DELTA && _rtt_gradient > 0) {
            //    rtt_diff = rtt_diff - PKTPAIR_DELTA;

            //    if (rtt_diff * 2 > _min_rtt) {
            //        rtt_diff = _min_rtt / 2;
            //    }

            //    _rate_estimate = llround(_rate_estimate * (1.0 - (PKTPAIR_BETA * rtt_diff) / _min_rtt));
            //}

            _rate_estimate = llround(_rate_estimate * (1.0 - _alpha / 2));
        }

        // If this was the first estimate, start sending packets at estimated rate.
        if (_state == STARTUP) {
            _state = NORMAL;
            _pktpair_ewma = pktpairdiff;
            _rate_estimate = (MSS_BYTES * 8 * timeFromSec(1)) / pktpairdiff;
            _bdp_estimate = _rate_estimate * timeAsSec(_min_rtt) / 8;
            if (_bdp_estimate < 2 * MSS_BYTES) {
                _bdp_estimate = 2 * MSS_BYTES;
            }
            sendPackets(current_ts);
        }
    } else {
        if (new_rtt > _prev_rtt) {
            _rtt_gradient = _rtt_gradient * (1 - RTT_GAIN) + (new_rtt - _prev_rtt) * RTT_GAIN;
        } else {
            _rtt_gradient = _rtt_gradient * (1 - RTT_GAIN) - (_prev_rtt - new_rtt) * RTT_GAIN;
        }
        _prev_rtt = new_rtt;
    }

    if (TRACE_FLOW == str()) {
        printf("%s RECV %.3lf rtt %.1lf seq %lu rate %.2lf bdp %lu lrtt %.1lf ppd %.03f inf %lu %f %u %u\n",
                str().c_str(), timeAsMs(current_ts),
                timeAsUs(_rtt), seqno, speedAsGbps(_rate_estimate),
                _bdp_estimate, timeAsUs(_prev_rtt),
                timeAsUs(_pktpair_ewma), (_highest_sent - _last_acked),
                _alpha, _marked_pkts, _total_pkts);
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
PacketPairSrc::sendPackets(simtime_picosec current_ts)
{
    // Send packets till _flowsize bytes.
    transmitPacketPair(current_ts);

    /* Schedule next transmission. Time to transmit 2*MSS_BYTES at estimated link rate. */
    simtime_picosec nextTransmission = timeFromSec((2.0 * MSS_BYTES * 8)/_rate_estimate);

    EventList::Get().sourceIsPendingRel(*this, nextTransmission);
}

void
PacketPairSrc::transmitPacketPair(simtime_picosec current_ts)
{
    // Don't send more packets it we have already sent _flowsize bytes.
    if (_flowsize != 0 && _highest_sent >= _flowsize) {
        return;
    }

    // Don't send more packets if we have more than BDP bytes in flight.
    uint64_t bdp_limit = _bdp_estimate + _bdp_estimate/2 + _dupacks * MSS_BYTES;
    if (_bdp_estimate != 0 && (_highest_sent - _last_acked) > bdp_limit) {
        //if (_bdp_estimate == 2 * MSS_BYTES) {
        //    cout << str() << " Limited BDP " << speedAsGbps(_rate_estimate)
        //         << " at " << timeAsUs(current_ts)
        //         << " inf " << _highest_sent - _last_acked
        //         << " limit " << bdp_limit << endl;
        //}
        return;
    }

    if (TRACE_FLOW == str()) {
        cout << str() << " SEND " << timeAsMs(current_ts) << " " << _highest_sent
             << " " << _last_acked << " " << (_highest_sent - _last_acked) << endl;
    }

    DataPacket *p;

    // Send out first packet.
    p = DataPacket::newpkt(_flow, *_route_fwd, _highest_sent + 1, MSS_BYTES);
    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);
    p->set_ts(current_ts);
    p->setFlag(Packet::PP_FIRST);
    p->sendOn();

    _highest_sent += MSS_BYTES;
    _packets_sent += MSS_BYTES;

    if (_rto_timeout == 0) {
        _rto_timeout = current_ts + _rto;
    }

    if (_flowsize != 0 && _highest_sent >= _flowsize) {
        return;
    }

    // Send out second packet at the same time (assuming source can
    // transmit at inifinite speed here!)
    p = DataPacket::newpkt(_flow, *_route_fwd, _highest_sent + 1, MSS_BYTES);
    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);
    p->set_ts(current_ts);
    p->sendOn();

    _highest_sent += MSS_BYTES;
    _packets_sent += MSS_BYTES;
}

void
PacketPairSrc::retransmitPacket(simtime_picosec current_ts)
{
    if (TRACE_FLOW == str()) {
        cout << str() << " RETX " << timeAsMs(current_ts) << " " << _last_acked << endl;
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


PacketPairSink::PacketPairSink()
    : DataSink(),
      _pktpairdiff(0),
      _first_pair_ts(0),
      _first_pair_seqno(0)
{
    // constructor
}

void
PacketPairSink::receivePacket(Packet& pkt)
{
    DataPacket *p = (DataPacket*)(&pkt);
    DataPacket::seq_t seqno = p->seqno();
    simtime_picosec ts = p->ts();
    processDataPacket(*p);

    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_RCVDESTROY);
    p->free();

    // If this is the first pair, record timestamp for pkt-pair calculation.
    if (p->getFlag(Packet::PP_FIRST)) {
        _pktpairdiff = 0;
        _first_pair_ts = EventList::Get().now();
        _first_pair_seqno = seqno;
    } else {
        if (_first_pair_seqno == seqno - MSS_BYTES) {
            // Check if this the second packet of the pair above.
            _pktpairdiff = EventList::Get().now() - _first_pair_ts;
        } else {
            // Erase old information to prevent miscalculation.
            _pktpairdiff = 0;
        }

        if (_pktpairdiff > 0 && _pktpairdiff < 800000) {
            cout << str() << " WTF! " << seqno << " " << EventList::Get().now()
                 << " " << _first_pair_ts << " " << _pktpairdiff << " " << p->size() << endl;
        }

        _first_pair_ts = 0;
        _first_pair_seqno = 0;
    }

    if (TRACE_FLOW == _src->str()) {
        cout << _src->str() << " SINK-TS: " << timeAsMs(EventList::Get().now()) << " at " << seqno
             << " PPD " << timeAsUs(_pktpairdiff) << " ECN " << (int)p->getFlag(Packet::ECN_FWD) << endl;
    }

    DataAck *ack = DataAck::newpkt(_src->_flow, *_route, _pktpairdiff, _cumulative_ack);
    ack->flow().logTraffic(*ack, *this, TrafficLogger::PKT_CREATESEND);
    ack->set_ts(ts);
    if (p->getFlag(Packet::ECN_FWD)) {
        ack->setFlag(Packet::ECN_REV);
    }
    ack->sendOn();
}

