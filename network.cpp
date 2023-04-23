/*
 * Network
 */
#include "network.h"

uint32_t Logged::LASTIDNUM = 1;

void
Packet::set(PacketFlow &flow,
            route_t &route,
            mem_b pkt_size,
            packetid_t id)
{
    _flow = &flow;
    _route = &route;
    _size = pkt_size;
    _id = id;
    _nexthop = 0;
    _flags = 0;
    _priority = 0;
}

void
Packet::sendOn()
{
    assert(_nexthop<_route->size());

    PacketSink *nextsink = (*_route)[_nexthop];
    _nexthop++;

    nextsink->receivePacket(*this);
}

PacketFlow::PacketFlow(TrafficLogger *logger)
                      : Logged("PacketFlow"), 
                      _nPackets(0), 
                      _logger(logger)
{}

void
PacketFlow::logTraffic(Packet &pkt, 
                       Logged &location, 
                       TrafficLogger::TrafficEvent ev)
{
    if (_logger) {
        _logger->logTraffic(pkt, location, ev);
    }
}
