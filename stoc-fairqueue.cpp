#include "stoc-fairqueue.h"

#define TRACE_PKT 0 && 221759 //281594

using namespace std;

StocFairQueue::StocFairQueue(linkspeed_bps bitrate, mem_b maxsize,
        QueueLogger *logger, uint32_t nQueue, uint32_t quantum)
    : Queue(bitrate, maxsize, logger),
      _nQueue(nQueue), _nPackets(0), _quantum(quantum)
{
    // Create the list of FIFO queues.
    _packets = vector<list<Packet*> >(_nQueue, list<Packet*>());

    // Initialize state vectors.
    _credits  = vector<uint32_t>(_nQueue, 0);
    _Qsize    = vector<uint32_t>(_nQueue, 0);
    _isActive = vector<bool>(_nQueue, false);
}

void
StocFairQueue::beginService()
{
    if (_nPackets > 0) {
        // We are guaranteed to have an active queue.
        uint32_t queue;
        while (true) {
            queue = _activeQ.front();
            if (_credits[queue] < _packets[queue].back()->size()) {
                // Not enough credit, bump to back of queue.
                _activeQ.pop_front();
                _activeQ.push_back(queue);
                _credits[queue] += _quantum;
            } else {
                break;
            }
        }
        EventList::Get().sourceIsPendingRel(*this, drainTime(_packets[queue].back()));
    }
}

void
StocFairQueue::completeService()
{
    assert(_nPackets > 0);

    uint32_t queue = _activeQ.front();
    Packet *pkt = _packets[queue].back();
    
    // Dequeue and book-keeping.
    _packets[queue].pop_back();
    _credits[queue] -= pkt->size();
    _Qsize[queue] -= pkt->size();
    _queuesize -= pkt->size();
    _nPackets -= 1;

    // If queue is empty, remove it from active list.
    if (_packets[queue].empty()) {
        _isActive[queue] = false;
        _activeQ.pop_front();
        _credits[queue] = 0;
    }

    pkt->flow().logTraffic(*pkt, *this, TrafficLogger::PKT_DEPART);
    if (_logger) {
        _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);
    }

    applyEcnMark(*pkt);
    pkt->sendOn();

    beginService();
}

void
StocFairQueue::receivePacket(Packet &pkt) 
{
    if (TRACE_PKT == pkt.flow().id) {
        cout << str() << " Pkt arrive " << timeAsMs(EventList::Get().now()) << " flowid " << pkt.flow().id << " " << pkt.id() << endl;
        cout << str() << " Current qsize " << _queuesize << " with " << _nPackets << " pkts " << pkt.size() << endl;
    }

    // If there is no space in the buffer, return immediately.
    if (_queuesize + pkt.size() > _maxsize) {
        if (TRACE_PKT == pkt.flow().id) {
            cout << str() <<  " DROP\n";
        }
        dropPacket(pkt);
        return;
    }

    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);
    bool queueWasEmpty = (_nPackets == 0);

    uint32_t queue = hashFlow(0, pkt.flow().id) % _nQueue;

    // Enqueue it.
    _packets[queue].push_front(&pkt);
    _Qsize[queue] += pkt.size();
    _queuesize += pkt.size();
    _nPackets += 1;

    // If FIFO queue wasn't active, make it active.
    if (!_isActive[queue]) {
        _isActive[queue] = true;
        _activeQ.push_back(queue);
        _credits[queue] = _quantum;
    }

    // If queue was empty, schedule next departure.
    if (queueWasEmpty) {
        assert(_nPackets == 1);
        beginService();
    }
}

void
StocFairQueue::dropPacket(Packet &pkt)
{
    if (_logger) {
        _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
    }
    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_DROP);
    pkt.free();
}

uint64_t
StocFairQueue::hashFlow(int index, uint32_t flowid)
{
    /* A lame hash function impersonator, multiplies by a large prime number. */

    uint64_t prime;

    switch (index) {
        case 0:
            prime = 7643;   // 970th prime number.
            break;

        case 1:
            prime = 7723;   // 980th prime number.
            break;

        case 2:
            prime = 7829;   // 990th prime number.
            break;

        case 3:
            prime = 7919;   // 1000th prime number.
            break;

        default:
            prime = 1;
    }

    return prime * flowid;
}

void
StocFairQueue::printStats()
{
}
