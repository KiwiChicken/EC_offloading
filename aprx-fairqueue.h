#ifndef APRX_FAIR_QUEUE_H
#define APRX_FAIR_QUEUE_H

/*
 * An approximate fair-queue that emulates byte-by-byte round robin.
 */

#include "queue.h"

#define ECN_MARK_ROUND 8

struct AFQcfg {
    // Default values.
    AFQcfg() : nHash(2), nBucket(1024), nQueue(32), bytesPerRound(MSS_BYTES), alpha(8) {}

    uint32_t nHash;         // Rows in the count-min sketch.
    uint32_t nBucket;       // Columns in the count-min sketch.
    uint32_t nQueue;        // Number of available FIFO queues. 
    uint32_t bytesPerRound; // Bytes of a flow to be enqueued in a queue.
    uint32_t alpha;         // Coefficient for dymanic buffer sharing.
};

class AprxFairQueue : public Queue
{
public:
    AprxFairQueue(linkspeed_bps bitrate, mem_b maxsize,
                    QueueLogger *logger, struct AFQcfg config = AFQcfg());
    void receivePacket(Packet &pkt);
    void printStats();

protected:
    void beginService();
    void completeService();
    void dropPacket(Packet &pkt);

private:
    uint64_t hashFlow(int index, uint32_t flowid);

    // Multiple queues storing all the packets.
    std::vector<std::list<Packet*> > _packets;

    // Count-min sketch to store bytes transmitted by a flow.
    std::vector<std::vector<uint64_t> > _sketch;

    // Bytes stored in each FIFO queue.
    std::vector<uint32_t> _Qsize;

    // AFQ Parametes.
    struct AFQcfg _cfg;

    // Current queue being serviced.
    uint32_t _currQ;

    // Number of rounds elapsed
    // A round is equivalent to servicing a queue till it's empty.
    uint64_t _nRounds;

    // Number of packets currently enqueued.
    uint64_t _nPackets;

    std::unordered_map<uint32_t, uint64_t> _exactBytes;
    uint64_t _error;
    uint64_t _count;
    uint64_t _zero;
};

#endif
