#ifndef EXOQUEUE_H
#define EXOQUEUE_H

/*
 * A simple exogenous queue
 */

#include "network.h"

class ExoQueue : public PacketSink
{
public:
    ExoQueue(double loss_rate);
    void receivePacket(Packet &pkt);
    void setLossRate(double l);

    // Housekeeping
    double _loss_rate;
};

#endif
