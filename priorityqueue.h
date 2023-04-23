#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

/*
 * A priority-queue that drains packets based on their priority.
 */

#include "queue.h"

#include <set>

class ComparePacketPriority
{
public:
    bool operator() (Packet *a, Packet *b)
    {
        if (a->getPriority() < b->getPriority()) {
            return true;
        } else {
            return false;
        }
    }
};

class PriorityQueue : public Queue
{
public:
    PriorityQueue(linkspeed_bps bitrate, mem_b maxsize, QueueLogger *logger);
    void receivePacket(Packet &pkt);
    void printStats();

protected:
    void beginService();
    void completeService();

private:
    // Multi-set of all packets, to transmit from head or drop from tail.
    std::multiset<Packet*, ComparePacketPriority> _packets;

    // Current packet being serviced.
    Packet *_currentPkt;
};

#endif
