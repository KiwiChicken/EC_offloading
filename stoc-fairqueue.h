#ifndef STOC_FAIR_QUEUE_H
#define STOC_FAIR_QUEUE_H

/*
 * An stochastic fair-queue using DRR.
 */

#include "queue.h"

class StocFairQueue : public Queue
{
public:
    StocFairQueue(linkspeed_bps bitrate, mem_b maxsize,
            QueueLogger *logger, uint32_t nQueue = 32, uint32_t quantum = MSS_BYTES);
    void receivePacket(Packet &pkt);
    void printStats();

protected:
    void beginService();
    void completeService();
    void dropPacket(Packet &pkt);

private:
    uint64_t hashFlow(int index, uint32_t flowid);

    // Number of FIFO queues.
    uint32_t _nQueue;

    // Number of packets currently enqueued.
    uint64_t _nPackets;
    
    // Quantum to transmit in each round.
    uint32_t _quantum;

    // Multiple queues storing all the packets.
    std::vector<std::list<Packet*> > _packets;

    // Credits available for each queue.
    std::vector<uint32_t> _credits;

    // Bytes stored in each FIFO queue.
    std::vector<uint32_t> _Qsize;

    // Flag for whether a queue is active or not.
    std::vector<bool> _isActive;

    // List of active queues.
    std::list<uint32_t> _activeQ;
};

#endif
