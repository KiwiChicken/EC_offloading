#include "pipe.h"

using namespace std;

Pipe::Pipe(simtime_picosec delay)
    : EventSource("pipe"), _delay(delay)
{}

void
Pipe::receivePacket(Packet &pkt)
{
    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);

    if (_inflight.empty()) {
        // no packets currently inflight.
        // need to notify the eventlist we've an event pending
        EventList::Get().sourceIsPendingRel(*this, _delay);
    }

    _inflight.push_front(make_pair(EventList::Get().now() + _delay, &pkt));
}

void
Pipe::doNextEvent()
{
    if (_inflight.size() == 0) {
        return;
    }

    Packet *pkt = _inflight.back().second;
    _inflight.pop_back();
    pkt->flow().logTraffic(*pkt, *this, TrafficLogger::PKT_DEPART);
    pkt->sendOn();

    if (!_inflight.empty()) {
        // notify the eventlist we've another event pending
        simtime_picosec nexteventtime = _inflight.back().first;
        EventList::Get().sourceIsPending(*this, nexteventtime);
    }
}
