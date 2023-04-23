/*
 * Simulator eventlist
 */
#include "eventlist.h"

using namespace std;

EventList *EventList::instance = NULL;

EventList&
EventList::Get()
{
    if (instance == NULL) {
        instance = new EventList;
        instance->_nEventsProcessed = 0;
        instance->_endtime = 0;
        instance->_lasteventtime = 0;
    }
    return *instance;
}

void
EventList::setEndtime(simtime_picosec endtime)
{
    _endtime = endtime;
}

bool
EventList::doNextEvent() 
{
    if (_pendingsources.empty()) {
        return false;
    }

    simtime_picosec nexteventtime = _pendingsources.begin()->first;
    EventSource *nextsource = _pendingsources.begin()->second;
    _pendingsources.erase(_pendingsources.begin());

    assert(nexteventtime >= _lasteventtime);

    // set this before calling doNextEvent, so that this::now() is accurate
    _lasteventtime = nexteventtime;

    // Measure how much time event takes for debugging purposes.
    struct timespec t1, t2;
    if (DEBUG_HTSIM) {
        clock_gettime(CLOCK_MONOTONIC, &t1);
    }

    // Process the event.
    nextsource->doNextEvent();
    _nEventsProcessed++;

    if (DEBUG_HTSIM) {
        clock_gettime(CLOCK_MONOTONIC, &t2);
        uint64_t diff = (t2.tv_sec - t1.tv_sec) * 1000000000 + (t2.tv_nsec - t1.tv_nsec);
        string id = nextsource->str();
        if (id.find_first_of("0123456789") != string::npos) {
            id.erase(id.find_first_of("0123456789"));
        }
        if (_stats.find(id) == _stats.end()) {
            _stats[id] = make_pair(1, diff/1.0);
        } else {
            _stats[id].second = _stats[id].second * _stats[id].first;
            _stats[id].first += 1;
            _stats[id].second = (_stats[id].second + diff) / _stats[id].first;
        }
    }

    return true;
}

void 
EventList::sourceIsPending(EventSource &src,
                           simtime_picosec when) 
{
    assert(when >= now());

    if (_endtime == 0 || when <= _endtime) {
        _pendingsources.insert(make_pair(when, &src));
    }
}
