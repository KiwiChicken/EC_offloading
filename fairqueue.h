#ifndef FAIR_QUEUE_H
#define FAIR_QUEUE_H

/*
 * A fair-queue that emulates byte-by-byte round robin.
 */

#include "queue.h"

#include <set>

class CompareFqPackets
{
public:
    bool operator() (std::pair<uint64_t,Packet*> a,
                     std::pair<uint64_t,Packet*> b)
    {
        if (a.first < b.first) {
            return true;
        } else if (a.first > b.first) {
            return false;
        } else {
            if (a.second->id() < b.second->id()) {
                return true;
            } else {
                return false;
            }
        }
    }
};

class FairQueue : public Queue
{
public:
    FairQueue(linkspeed_bps bitrate, mem_b maxsize, QueueLogger *logger);
    void receivePacket(Packet &pkt);
    void printStats();

protected:
    void beginService();
    void completeService();

private:
    // Updates the current round number based on time elapsed and active flows.
    void updateRoundNumber();

    // Multi-set of all packets, to transmit from head or drop from tail.
    std::multiset<std::pair<uint64_t,Packet*>, CompareFqPackets> _packets;

    // Finish round number of each active flow.
    std::unordered_map<uint32_t, uint64_t> _flowRound;

    // Number of packets enqueued for each active flow.
    std::unordered_map<uint32_t, uint32_t> _nPackets;

    simtime_picosec _roundUpdate; // Last round update time.
    uint32_t _nActiveFlows;       // Number of active flows.
    uint64_t _roundNumber;        // Current round number.
    double _exactRoundNumber;     // Current round number in decimals.

    Packet *_currentPkt;          // Current packet being serviced.

    enum Mode {
        PRECISE,
        LAZY
    } _mode;
};

#endif
