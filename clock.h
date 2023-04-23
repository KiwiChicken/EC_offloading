/*
 * Simulator clock header
 */
#ifndef CLOCK_H
#define CLOCK_H

/*
 * A convenient item to put into an eventlist: it displays a tick mark 
 * every so often, to show the simulation is running.
 */

#include "eventlist.h"

#define GAIN 0.75
#define INIT_ESTIMATE timeFromUs(1)

class Clock : public EventSource
{
    public:
        Clock(simtime_picosec estimate = INIT_ESTIMATE);
        void doNextEvent();
    private:
        // An estimate of how far the simulation moves in 1 sec of realtime.
        simtime_picosec _estimate;

        // Realtime elapsed since the beginning.
        double _elapsedRT;

        // State stored at each major tick.
        simtime_picosec _simTime;
        uint64_t _nEvents;
        double _realTime;

        // Time of last minor tick.
        struct timespec _lastTick;

        // Number of minor ticks.
        int _ticks;
};

#endif /* CLOCK_H */
