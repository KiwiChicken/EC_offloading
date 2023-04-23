/*
 * pipe header
 *   - A pipe is a dumb device which simply delays all incoming packets;
 */
#ifndef PIPE_H
#define PIPE_H

#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"

#include <deque>

class Pipe : public EventSource, public PacketSink
{
    public:
        Pipe(simtime_picosec delay);
        void receivePacket(Packet &pkt); // inherited from PacketSink
        void doNextEvent(); // inherited from EventSource
        simtime_picosec delay() { return _delay; }

    private:
        simtime_picosec _delay;
        typedef std::pair<simtime_picosec,Packet *> pktrecord_t;
        std::deque<pktrecord_t> _inflight; // the packets in flight (or being serialized)
};

#endif /* PIPE_H */
