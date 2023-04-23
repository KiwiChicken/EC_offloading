/*
 * DataSink
 */
#include "datasink.h"

using namespace std;

DataSink::DataSink() : Logged("datasink"), _cumulative_ack(0) {}

void 
DataSink::connect(DataSource &src, route_t &route)
{
    _src = &src;
    _route = &route;
}

void 
DataSink::processDataPacket(DataPacket &pkt)
{
    mem_b size = pkt.size();
    DataPacket::seq_t seqno = pkt.seqno();

    if (seqno == _cumulative_ack + 1) {
        // It's the next expected sequence number.
        _cumulative_ack = seqno + size - 1;

        // Are there any additional received packets that we can now ack?
        while (!_received.empty()) {
            pair<DataAck::seq_t,mem_b> packet = _received.front();

            if (_cumulative_ack + 1 == packet.first) {
                _received.pop_front();
                _cumulative_ack += packet.second;
            } else {
                break;
            }
        }
    } else if (seqno <= _cumulative_ack) {
        // Must have been a bad retransmit, do nothing.
    } else {
        // It's not the next expected sequence number.
        if (_received.empty()) {
            _received.push_back(make_pair(seqno, size));
        } else if (seqno > _received.back().first) {
            _received.push_back(make_pair(seqno, size));
        } else {
            // Insert the packet in the right position.
            for (auto i = _received.begin(); i != _received.end(); i++) {
                if (seqno == i->first) {
                    // Probably a bad retransmit.
                    break;
                } else if (seqno < i->first) {
                    _received.insert(i, make_pair(seqno, size));
                    break;
                }
            }
        }
    }
}

DataAck::seq_t 
DataSink::cumulative_ack()
{
    return _cumulative_ack;
}

uint32_t 
DataSink::drops()
{
    return 0;
}
