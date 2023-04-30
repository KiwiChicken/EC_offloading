#include "pipe.h"
#include "logfile.h"
#include "loggers.h"
#include "test.h"
#include "aprx-fairqueue.h"
#include "fairqueue.h"
#include "priorityqueue.h"
#include "stoc-fairqueue.h"

#ifndef CONGA_H_
#define CONGA_H_

class CongaTopology {

public: 
    constexpr static int N_CORE = 12;       // In full network
    constexpr static int N_LEAF = 24;       // In full network
    constexpr static int N_SERVER = 32;     // Per leaf

    constexpr static int N_NODES = N_SERVER * N_LEAF;

    constexpr static uint64_t LEAF_BUFFER = 512000;
    constexpr static uint64_t CORE_BUFFER = 1024000;
    constexpr static uint64_t ENDH_BUFFER = 8192000;

    constexpr static uint64_t LEAF_SPEED = 10000000000; // 10gbps
    constexpr static uint64_t CORE_SPEED = 40000000000; // 40gbps

    constexpr static double LINK_DELAY = 0.1; // in microsec

    Pipe  *pCoreLeaf[N_CORE][N_LEAF];
    Queue *qCoreLeaf[N_CORE][N_LEAF];

    Pipe  *pLeafCore[N_CORE][N_LEAF];
    Queue *qLeafCore[N_CORE][N_LEAF];

    Pipe  *pLeafServer[N_LEAF][N_SERVER];
    Queue *qLeafServer[N_LEAF][N_SERVER];

    Pipe  *pServerLeaf[N_LEAF][N_SERVER];
    Queue *qServerLeaf[N_LEAF][N_SERVER];

    // Pipe  *pFPGACore[N_CORE];
    // Queue *qFPGACore[N_CORE];

    // Pipe  *pCoreFPGA[N_CORE];
    // Queue *qCoreFPGA[N_CORE];
    Pipe  *pToFPGA[N_CORE];
    Queue *qToFPGA[N_CORE];

    Pipe  *pFromFPGA[N_CORE];
    Queue *qFromFPGA[N_CORE];

    //TODO: decide core-fpga params

    void genTopology(bool is_ec_offload, std::string &queueType, Logfile &logfile);
    

private: 
    void createQueue(std::string &qType, Queue *&queue, uint64_t speed, uint64_t buffer, Logfile &lf);
    void genFPGA(bool is_ec_offload, std::string &queueType, Logfile &logfile);
};

#endif
