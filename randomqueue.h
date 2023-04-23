#ifndef RANDOM_QUEUE_H
#define RANDOM_QUEUE_H
/*
 * A simple FIFO queue that drops randomly when it gets full
 */

#include "queue.h"

class RandomQueue : public Queue
{
public:
    RandomQueue(linkspeed_bps bitrate, mem_b maxsize, QueueLogger *logger, mem_b drop);
    void receivePacket(Packet &pkt);
    void set_packet_loss_rate(double v);

private:
    mem_b _drop_th,_drop;
    int _buffer_drops;
    double _plr;
};

#endif
