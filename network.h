/*
 * Network header
 */
#ifndef NETWORK_H
#define NETWORK_H

#include "htsim.h"
#include "loggertypes.h"

#include <vector>

class Packet;
class PacketFlow;
class PacketSink;
typedef std::vector<PacketSink*> route_t;
typedef std::vector<route_t*> routes_t;
typedef uint32_t packetid_t;

// See datapacket.h to illustrate how Packet is typically used.
class Packet
{
    friend class PacketFlow;
    public:
    /* Flag types. */
    enum PacketFlag {
        ECN_FWD = 0,
        ECN_REV = 1,
        PP_FIRST = 2,
        DEADLINE = 3
    };

    Packet() {};
    virtual ~Packet() {};

    // Free the packet and return it to packet pool.
    virtual void free() = 0;

    // Send the packet to next hop.
    virtual void sendOn();

    // Return protected members.
    mem_b size() const {return _size;}
    PacketFlow& flow() const {return *_flow;}
    inline packetid_t id() const {return _id;}

    inline void setFlag(PacketFlag flag) {_flags = _flags | (1 << flag);}
    inline void unsetFlag(PacketFlag flag) {_flags = _flags & ~(1 << flag);}
    inline uint8_t getFlag(PacketFlag flag) {return (_flags & (1 << flag)) ? 1 : 0;}

    inline void setPriority(uint32_t p) {_priority = p;}
    inline uint32_t getPriority() {return _priority;}

    protected:
    void set(PacketFlow &flow, route_t &route, mem_b pkt_size, packetid_t id);

    PacketFlow *_flow;
    route_t *_route;
    mem_b _size;
    packetid_t _id;

    uint32_t _nexthop;

    uint32_t _flags;
    uint32_t _priority;
};

class PacketFlow : public Logged
{
    friend class Packet;
    public:
    PacketFlow(TrafficLogger *logger);
    virtual ~PacketFlow() {};
    void logTraffic(Packet &pkt, Logged &location, TrafficLogger::TrafficEvent ev);

    // How many packets of this flow are alive.
    uint32_t _nPackets;

    protected:
    TrafficLogger *_logger;
};

class PacketSink
{
    public:
        PacketSink() {}
        virtual ~PacketSink() {}
        virtual void receivePacket(Packet& pkt) = 0;
};


// For speed, it may be useful to keep a database of all packets that
// have been allocated -- that way we don't need a malloc for every
// new packet, we can just reuse old packets. Care, though -- the set()
// method will need to be invoked properly for each new/reused packet

template<class P>
class PacketDB
{
    public:
        P* allocPacket() {
            if (_freelist.empty()) 
                return new P();
            else {
                P* p = _freelist.back();
                _freelist.pop_back();
                return p;
            }
        };

        void freePacket(P* pkt) {
            _freelist.push_back(pkt);
        };
    protected:
        std::vector<P*> _freelist; // Irek says it's faster with vector than with list
};

#endif /* NETWORK_H */
