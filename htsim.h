/*
 * Simulator parameters
 */
#ifndef HTSIM_H
#define HTSIM_H

#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <string>
#include <iomanip>
#include <iostream>

/* Print stats on events processed. */
#define DEBUG_HTSIM 0

/* Some global definitions. */
#define MIN_RTO_US  200       // Min RTO in micro-sec
#define INIT_RTO_US 2500      // Initial RTO

#define MSS_BYTES 1500        // Max segment size in bytes
#define ACK_SIZE  40          // ACK size in bytes

#define BDP_BYTES 12500       // Bandwidth-delay product in bytes
#define BDP_PACKETS 12

#define ENABLE_ECN 1          // ECN enabled on queues or not

/* Units */
typedef uint64_t simtime_picosec;
typedef uint64_t linkspeed_bps;
typedef uint64_t mem_b;
typedef uint32_t addr_t;
typedef uint16_t port_t;


/* Random generators. */
inline double 
drand()
{
    int r = rand();
    int m = RAND_MAX;
    double d = (double)r/(double)m;
    return d;
}

inline int 
pareto(double alpha, int mean)
{
    // alpha is also called 'shape'
    double scale = (mean * (alpha - 1)) / alpha;
    return (int)(scale / pow(drand(), 1/alpha));
}

inline double 
exponential(double lambda)
{
    // mean is 1/lambda
    return -log(drand())/lambda;
}


/* Time conversions. */
inline simtime_picosec 
timeFromSec(double secs)
{
    simtime_picosec psecs = llround(secs * 1000000000000);
    return psecs;
}

inline simtime_picosec 
timeFromMs(double msecs)
{
    simtime_picosec psecs = llround(msecs * 1000000000);
    return psecs;
}

inline simtime_picosec 
timeFromUs(double usecs)
{
    simtime_picosec psecs = llround(usecs * 1000000);
    return psecs;
}

inline simtime_picosec 
timeFromNs(double nsecs)
{
    simtime_picosec psecs = llround(nsecs * 1000);
    return psecs;
}

inline double 
timeAsSec(simtime_picosec ps)
{
    double s_ = ps / 1000000000000.0L;
    return s_;
}

inline double 
timeAsMs(simtime_picosec ps)
{
    double ms_ = ps / 1000000000.0L;
    return ms_;
}

inline double 
timeAsUs(simtime_picosec ps)
{
    double us_ = ps / 1000000.0L;
    return us_;
}

inline double 
timeAsNs(simtime_picosec ps)
{
    double ns_ = ps / 1000.0L;
    return ns_;
}


/* Speed conversions. */
inline linkspeed_bps 
speedFromGbps(double Gbitps)
{
    linkspeed_bps bps = llround(Gbitps * 1000000000);
    return bps;
}

inline linkspeed_bps 
speedFromMbps(double Mbitps)
{
    linkspeed_bps bps = llround(Mbitps * 1000000);
    return bps;
}

inline linkspeed_bps 
speedFromKbps(double Kbitps)
{
    linkspeed_bps bps = llround(Kbitps * 1000);
    return bps;
}

inline double 
speedAsGbps(linkspeed_bps bps)
{
    double gbps = bps / 1000000000.0L;
    return gbps;
}

inline double 
speedAsMbps(linkspeed_bps bps)
{
    double mbps = bps / 1000000.0L;
    return mbps;
}

inline double 
speedAsKbps(linkspeed_bps bps)
{
    double kbps = bps / 1000.0L;
    return kbps;
}


/* Packet/speed conversion. */
inline linkspeed_bps 
speedFromPktps(uint64_t packetsPerSec)
{
    linkspeed_bps bps = packetsPerSec * 8 * MSS_BYTES;
    return bps;
}

inline double 
speedAsPktps(linkspeed_bps bps)
{
    double pktps = bps / (8.0L * MSS_BYTES);
    return pktps;
}

#endif /* HTSIM_H */
