#include "fairqueue.h"

#define TRACE_PKT 0 && 16829

using namespace std;

FairQueue::FairQueue(linkspeed_bps bitrate, mem_b maxsize, QueueLogger *logger)
    : Queue(bitrate, maxsize, logger), _roundUpdate(0),
      _nActiveFlows(0), _roundNumber(0), _exactRoundNumber(0.0)
{
    _mode = LAZY;
}

void
FairQueue::beginService()
{
    if (!_packets.empty()) {
        // Alternate way of updating round number.
        if (_mode == LAZY) {
            _roundNumber = _packets.begin()->first;
        }

        // Remove packet from the queue for transmit.
        _currentPkt = _packets.begin()->second;
        _packets.erase(_packets.begin());

        // Schedule it's completion time.
        EventList::Get().sourceIsPendingRel(*this, drainTime(_currentPkt));

        if (TRACE_PKT == _currentPkt->flow().id) {
            cout << str() << " Pkt depart sched " << EventList::Get().now() << " " << _roundNumber << " "
                 << _currentPkt->id() << " " << _packets.size() << " " << drainTime(_currentPkt)
                 << " " << _currentPkt->size() << " " << _ps_per_byte << endl;
        }
    }
}

void
FairQueue::completeService()
{
    // Update the packet count for this flow.
    uint32_t flowid = _currentPkt->flow().id;

    if (_nPackets[flowid] == 1) {
        _nPackets.erase(flowid);
        _flowRound.erase(flowid);
    } else {
        _nPackets[flowid] = _nPackets[flowid] - 1;
    }

    // Logging and cleanup.
    _currentPkt->flow().logTraffic(*_currentPkt, *this, TrafficLogger::PKT_DEPART);
    if (_logger) {
        _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *_currentPkt);
    }

    if (TRACE_PKT == _currentPkt->flow().id) {
        cout << str() << " Pkt depart " << EventList::Get().now() << " " << _roundNumber << " "
             << _currentPkt->id() << " " << _currentPkt->size() << " " << _packets.size() << endl;
    }

    // Fair-queue shouldn't need ECN marks as it drops the most "unfair" packet.
    applyEcnMark(*_currentPkt);
    _currentPkt->sendOn();

    // Clear packet being transmitted.
    _queuesize -= _currentPkt->size();
    _currentPkt = NULL;

    beginService();
}

void
FairQueue::receivePacket(Packet& pkt) 
{
    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);
    bool queueWasEmpty = (_currentPkt == NULL) && _packets.empty();

    // Update the current round number before we assign it to new packet.
    if (_mode == PRECISE) {
        updateRoundNumber();
        _roundNumber = (uint64_t)_exactRoundNumber;
    }

    if (TRACE_PKT == pkt.flow().id) {
        cout << str() << " Pkt arrive " << EventList::Get().now() << " " << _roundNumber
             << " " << pkt.id() << " " << pkt.size() << " " << _packets.size() << endl;
    }

    uint32_t flowid = pkt.flow().id;

    // Increment packet count for this flow.
    if (_nPackets.find(flowid) == _nPackets.end()) {
        _nPackets[flowid] = 0;
    }
    _nPackets[flowid] = _nPackets[flowid] + 1;

    // If the flow is not active, update round number and active flows.
    if (_flowRound.find(flowid) == _flowRound.end()) {
        _flowRound[flowid] = _roundNumber + pkt.size();
        _nActiveFlows = _nActiveFlows + 1;
    } else {
        if (_flowRound[flowid] > _roundNumber) {
            _flowRound[flowid] = _flowRound[flowid] + pkt.size();
        } else {
            _flowRound[flowid] = _roundNumber + pkt.size();
        }
    }

    _packets.insert (make_pair(_flowRound[flowid], &pkt));

    _queuesize += pkt.size();

    if (_logger) {
        _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);
    }

    // If we are over the queue limit, drop packets from the end.
    while (_queuesize > _maxsize) {
        Packet *p = prev(_packets.end())->second;
        _packets.erase(prev(_packets.end()));
        _queuesize -= p->size();

        // Update packet counts for dropped flow packet.
        uint32_t dropid = p->flow().id;

        if (_nPackets[dropid] == 1) {
            // This flow will become inactive due to drop.
            _flowRound.erase(dropid);
            _nPackets.erase(dropid);
            _nActiveFlows = _nActiveFlows - 1;
        } else {
            _flowRound[dropid] = _flowRound[dropid] - p->size();
            _nPackets[dropid] = _nPackets[dropid] - 1;
        }

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
FairQueue::updateRoundNumber()
{
    // Calculate link rate in bytes per picosec.
    static double LinkRate = (_bitrate / 8.0) / 1000000000000.0;

    while (_nActiveFlows > 0) {
        // Find the lowest finish round number of any active flow.
        uint64_t lowestRoundFinish = 1LL << 63;
        uint32_t lowestFlow = -1;

        // TODO: Do this more efficiently.
        for (auto x : _flowRound) {
            if (x.second < lowestRoundFinish) {
                lowestRoundFinish = x.second;
                lowestFlow = x.first;
            }
        }

        // Time elapsed since last round update in microseconds.
        uint64_t delta = EventList::Get().now() - _roundUpdate;

        //cout << "Time " << timeAsSec(eventlist().now()) << endl
        //     << "LinkRate: " << LinkRate << endl
        //     << "LowestRoundFinish: " << lowestRoundFinish << endl
        //     << "LowestFlow: " << lowestFlow << endl
        //     << "TimeDelta: " << delta << endl
        //     << "ActiveFlows: " << _nActiveFlows << endl << endl;

        // If the flow went inactive during the time elapsed, find what time and
        // update round number, number of active flows appropriately.
        if (lowestRoundFinish <= (_exactRoundNumber + (delta * LinkRate) / _nActiveFlows)) {
            _roundUpdate = _roundUpdate + (lowestRoundFinish - _exactRoundNumber) * _nActiveFlows / LinkRate;
            _exactRoundNumber = lowestRoundFinish;

            // Remove flow from active list.
            _flowRound.erase(lowestFlow);
            if (_nPackets[lowestFlow] != 0) {
                cout << _nPackets[lowestFlow] << " This should be zero!\n";
            }

            _nActiveFlows = _nActiveFlows - 1;
        } else {
           _exactRoundNumber = _exactRoundNumber + (delta * LinkRate) / _nActiveFlows;
           break;
        }
    }

    _roundUpdate = EventList::Get().now();
}

void
FairQueue::printStats()
{
    unordered_map<uint32_t, uint32_t> counts;

    for (auto const &i : _packets) {
        uint32_t fid = (i.second)->flow().id;
        if (counts.find(fid) == counts.end()) {
            counts[fid] = 0;
        }
        counts[fid] = counts[fid] + 1;
    }

    cout << str() << " " << timeAsMs(EventList::Get().now()) << " stats";
    for (auto it = counts.begin(); it != counts.end(); it++) {
        cout << " " << it->first << "->" << it->second;
    }
    cout << endl;
}
