#include "aprx-fairqueue.h"

#define TRACE_PKT 0 && 221759 //281594

using namespace std;

AprxFairQueue::AprxFairQueue(linkspeed_bps bitrate, mem_b maxsize,
        QueueLogger *logger, struct AFQcfg config)
    : Queue(bitrate, maxsize, logger)
{
    // Save the AFQ config parameters.
    _cfg = config;

    // Create the list of FIFO queues.
    _packets = vector<list<Packet*> >(_cfg.nQueue, list<Packet*>());

    _Qsize = vector<uint32_t>(_cfg.nQueue, 0);

    _sketch = vector<vector<uint64_t> >(_cfg.nHash, vector<uint64_t>(_cfg.nBucket, 0));

    _nRounds = 0;
    _nPackets = 0;

    _currQ = 0;

    _error = 0;
    _count = 0;
    _zero = 0;
}

void
AprxFairQueue::beginService()
{
    if (_nPackets > 0) {
        // We are guaranteed to hit a non-empty queue.
        while (_packets[_currQ].empty()) {
            _currQ++;
            _nRounds++;
            if (_currQ == _cfg.nQueue) {
                _currQ = 0;
            }
        }

        EventList::Get().sourceIsPendingRel(*this, drainTime(_packets[_currQ].back()));
    }
}

void
AprxFairQueue::completeService()
{
    assert(_nPackets > 0);

    Packet *pkt = _packets[_currQ].back();
    _packets[_currQ].pop_back();
    _Qsize[_currQ] -= pkt->size();
    _queuesize -= pkt->size();
    _nPackets -= 1;

    pkt->flow().logTraffic(*pkt, *this, TrafficLogger::PKT_DEPART);
    if (_logger) {
        _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);
    }

    uint64_t bytes = ULLONG_MAX;
    for (unsigned int i = 0; i < _cfg.nHash; i++) {
        uint64_t index = hashFlow(i, pkt->flow().id) % _cfg.nBucket;
        bytes = min(_sketch[i][index], bytes);
    }

    uint64_t flowRound = bytes/_cfg.bytesPerRound;
    if (flowRound - _nRounds >= ECN_MARK_ROUND) {
        pkt->setFlag(Packet::ECN_FWD);
    }

    applyEcnMark(*pkt);
    pkt->sendOn();

    beginService();
}

void
AprxFairQueue::receivePacket(Packet &pkt) 
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

    uint32_t flowid = pkt.flow().id;
    uint64_t bytes = ULLONG_MAX;

    // Do first pass of sketch to find bytes transmitted by this flow.
    for (unsigned int i = 0; i < _cfg.nHash; i++) {
        uint64_t index = hashFlow(i, flowid) % _cfg.nBucket;
        bytes = min(_sketch[i][index], bytes);
    }

    // Figure out which FIFO queue to place this packet in.
    uint64_t flowRound = bytes/_cfg.bytesPerRound;
    uint32_t outQ = -1;

    if (flowRound <= _nRounds) {
        // Flow hasn't sent for quite a while.
        bytes = max(_nRounds * _cfg.bytesPerRound, bytes);
        outQ = _currQ;
    } else if (flowRound - _nRounds >= _cfg.nQueue) {
        // Flow is sending too fast, packet too far in the future, DROP!
        if (TRACE_PKT == pkt.flow().id) {
            cout << str() <<  " DROP\n";
        }
        dropPacket(pkt);

        // We shouldn't increment the sketch to reflect a dropped packet.
        return;
    } else {
        // We must have free space for this packet. Find which queue.
        outQ = _currQ + (flowRound - _nRounds);
        if (outQ >= _cfg.nQueue) {
            outQ = outQ - _cfg.nQueue;
        }
    }

    if (TRACE_PKT == pkt.flow().id) {
        cout << str() << " Pkt depart " << timeAsMs(EventList::Get().now()) << " flowid " << pkt.flow().id << " " << pkt.id() << endl;
        cout << str() << " " << outQ << " " << _currQ << endl;
    }

    bytes += pkt.size();

    // Exact bytes.
    //if (_exactBytes.find(flowid) == _exactBytes.end()) {
    //    _exactBytes[flowid] = _nRounds * _cfg.bytesPerRound + pkt.size();
    //} else {
    //    if (_exactBytes[flowid] > _nRounds * _cfg.bytesPerRound) {
    //        _exactBytes[flowid] = _exactBytes[flowid] + pkt.size();
    //    } else {
    //        _exactBytes[flowid] = _nRounds * _cfg.bytesPerRound + pkt.size();
    //    }
    //}

    //// Measure error.
    //if (bytes == _exactBytes[flowid]) {
    //    _zero++;
    //} else if (bytes < _exactBytes[flowid]) {
    //    _error += _exactBytes[flowid] - bytes;
    //} else {
    //    _error += bytes - _exactBytes[flowid];
    //}
    //_count++;

    // Enqueue it!
    _packets[outQ].push_front(&pkt);
    _Qsize[outQ] += pkt.size();
    _queuesize += pkt.size();
    _nPackets += 1;

    // Update the sketch to reflect new bytes.
    for (unsigned int i = 0; i < _cfg.nHash; i++) {
        uint64_t index = hashFlow(i, flowid) % _cfg.nBucket;
        _sketch[i][index] = max(_sketch[i][index], bytes);
    }

    if (queueWasEmpty) {
        assert(_nPackets == 1);
        beginService();
    }
}

void
AprxFairQueue::dropPacket(Packet &pkt)
{
    if (_logger) {
        _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
    }
    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_DROP);
    pkt.free();
}

uint64_t
AprxFairQueue::hashFlow(int index, uint32_t flowid)
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
AprxFairQueue::printStats()
{
    unordered_map<uint32_t, uint32_t> counts;

    for (uint32_t i = 0; i < _cfg.nQueue; i++) {
        for (auto const &i : _packets[i]) {
            uint32_t fid = i->flow().id;
            if (counts.find(fid) == counts.end()) {
                counts[fid] = 0;
            }
            counts[fid] = counts[fid] + 1;
        }
    }

    cout << str() << " " << timeAsMs(EventList::Get().now()) << " stats";
    for (auto it = counts.begin(); it != counts.end(); it++) {
        cout << " " << it->first << "->" << it->second;
    }
    cout << endl;

    //cout << "AFQ: " << _error << " " << _count << " " << _zero << endl;
}
