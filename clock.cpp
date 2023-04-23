/*
 * Simulator clock
 */
#include "clock.h"

using namespace std;

Clock::Clock(simtime_picosec estimate)
    : EventSource("clock"), 
    _estimate(estimate),
    _elapsedRT(0.0), 
    _simTime(0), 
    _nEvents(0), 
    _realTime(0.0), 
    _ticks(0)
{
    clock_gettime(CLOCK_MONOTONIC, &_lastTick);

    EventList::Get().sourceIsPendingRel(*this, _estimate);
}

void
Clock::doNextEvent()
{
    _ticks++;

    // Find time elaped since last tick in ms.
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    double elapsed = (ts.tv_sec - _lastTick.tv_sec) +
        (ts.tv_nsec - _lastTick.tv_nsec) / 1000000000.0;

    _lastTick = ts;
    _elapsedRT += elapsed;

    // _estimate : elapsed :: ? : 1sec
    double newEstimate = (_estimate / elapsed);

    // Use some sort of interpolation for _estimate?
    _estimate = newEstimate;

    if (_ticks == 10) {
        _ticks = 0;

        uint64_t eventsPerSec = (uint64_t)(EventList::Get()._nEventsProcessed - _nEvents)
            / (_elapsedRT - _realTime) / 1000; 
        _nEvents = EventList::Get()._nEventsProcessed;
        _realTime = _elapsedRT;

        simtime_picosec current_ts = EventList::Get().now();
        simtime_picosec simDiff = current_ts - _simTime;
        _simTime = current_ts;

        fprintf(stderr, "| simtime  %9.6lf | realtime  %6.1lf | speed  %8.6lf  @ %5lu Kops/s |\n",
                timeAsSec(current_ts), _elapsedRT, timeAsSec(simDiff), eventsPerSec);

        if (DEBUG_HTSIM) {
            for (auto x : EventList::Get()._stats) {
                cerr << x.first << "\t" << x.second.first << "\t" << x.second.second << endl;
            }
        }
    } else {
        fprintf(stderr, ".");
    }

    EventList::Get().sourceIsPendingRel(*this, _estimate);
}
