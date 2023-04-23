#include "priorityqueue.h"

#define TRACE_PKT 0 && 4304

using namespace std;

PriorityQueue::PriorityQueue(linkspeed_bps bitrate, mem_b maxsize, QueueLogger *logger)
    : Queue(bitrate, maxsize, logger)
{
}

void
PriorityQueue::beginService()
{
    if (!_packets.empty()) {
        // Remove packet from the queue for transmit.
        _currentPkt = *_packets.begin();
        _packets.erase(_packets.begin());

        // Schedule it's completion time.
        EventList::Get().sourceIsPendingRel(*this, drainTime(_currentPkt));

        if (TRACE_PKT == _currentPkt->flow().id) {
            cout << str() << " Pkt depart sched " << EventList::Get().now() << " "
                 << _currentPkt->id() << " " << _packets.size() << " " << drainTime(_currentPkt)
                 << " " << _currentPkt->size() << " " << _ps_per_byte << endl;
        }
    }
}

void
PriorityQueue::completeService()
{
    // Logging and cleanup.
    _currentPkt->flow().logTraffic(*_currentPkt, *this, TrafficLogger::PKT_DEPART);
    if (_logger) {
        _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *_currentPkt);
    }

    if (TRACE_PKT == _currentPkt->flow().id) {
        cout << str() << " Pkt depart " << EventList::Get().now() << " " << _currentPkt->id()
             << " " << _currentPkt->size() << " " << _packets.size() << endl;
    }

    applyEcnMark(*_currentPkt);
    _currentPkt->sendOn();

    // Clear packet being transmitted.
    _queuesize -= _currentPkt->size();
    _currentPkt = NULL;

    beginService();
}

void
PriorityQueue::receivePacket(Packet& pkt) 
{
    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);
    bool queueWasEmpty = (_currentPkt == NULL) && _packets.empty();

    if (TRACE_PKT == pkt.flow().id) {
        cout << str() << " Pkt arrive " << EventList::Get().now() << " " << pkt.id() << " " << pkt.size() << " " << _packets.size() << endl;
    }

    _packets.insert(&pkt);
    _queuesize += pkt.size();

    if (_logger) {
        _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);
    }

    // If we are over the queue limit, drop packets from the end.
    while (_queuesize > _maxsize) {
        Packet *p = *prev(_packets.end());
        _packets.erase(prev(_packets.end()));
        _queuesize -= p->size();

        if (_logger) {
            _logger->logQueue(*this, QueueLogger::PKT_DROP, *p);
        }
        p->flow().logTraffic(*p, *this, TrafficLogger::PKT_DROP);
        p->free();
    }

    if (queueWasEmpty) {
        beginService();
    }
}

void
PriorityQueue::printStats()
{
    unordered_map<uint32_t, uint32_t> counts;

    for (auto const& i : _packets) {
        uint32_t fid = i->flow().id;
        if (counts.find(fid) == counts.end()) {
            counts[fid] = 0;
        }
        counts[fid] = counts[fid] + 1;
    }

    cout << str() << " stats ";
    for (auto it = counts.begin(); it != counts.end(); it++) {
        cout << " " << it->second;
    }
    cout << endl;
}
