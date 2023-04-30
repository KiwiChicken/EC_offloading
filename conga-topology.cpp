#include "conga-topology.h"

void CongaTopology::genTopology(bool is_ec_offload, std::string &queueType, Logfile &logfile) {
    // connect all cores to leaves
    for (int i = 0; i < N_CORE; i++) {
        for (int j = 0; j < N_LEAF; j++) {
            // Uplink
            createQueue(queueType, qLeafCore[i][j], CORE_SPEED, LEAF_BUFFER, logfile);
            qLeafCore[i][j]->setName("q-leaf-core-" + std::to_string(i) + "-" + std::to_string(j));
            logfile.writeName(*(qLeafCore[i][j]));

            // createPipe(pipeType, pLeafCore[i][j], LINK_DELAY);
            pLeafCore[i][j] = new Pipe(LINK_DELAY);
            pLeafCore[i][j]->setName("p-leaf-core-" + std::to_string(i) + "-" + std::to_string(j));
            logfile.writeName(*(pLeafCore[i][j]));

            // Downlink
            createQueue(queueType, qCoreLeaf[i][j], CORE_SPEED, CORE_BUFFER, logfile);
            qCoreLeaf[i][j]->setName("q-core-leaf-" + std::to_string(i) + "-" + std::to_string(j));
            logfile.writeName(*(qCoreLeaf[i][j]));

            // createPipe(pipeType, pCoreLeaf[i][j], LINK_DELAY);
            pCoreLeaf[i][j] = new Pipe(LINK_DELAY);
            pCoreLeaf[i][j]->setName("p-core-leaf-" + std::to_string(i) + "-" + std::to_string(j));
            logfile.writeName(*(pCoreLeaf[i][j]));
        }
    }

    // connect servers to leaf
    for (int i = 0; i < N_LEAF; i++) {
        for (int j = 0; j < N_SERVER; j++) {
            // Uplink
            createQueue(queueType, qLeafServer[i][j], LEAF_SPEED, ENDH_BUFFER, logfile);
            qLeafServer[i][j]->setName("q-leaf-server-" + std::to_string(i) + "-" + std::to_string(j));
            logfile.writeName(*(qLeafServer[i][j]));

            // createPipe(pipeType, pLeafServer[i][j], LINK_DELAY);
            pLeafServer[i][j] = new Pipe(LINK_DELAY);
            pLeafServer[i][j]->setName("p-leaf-server" + std::to_string(i) + "-" + std::to_string(j));
            logfile.writeName(*(pLeafServer[i][j]));

            // Downlink
            createQueue(queueType, qServerLeaf[i][j], LEAF_SPEED, LEAF_BUFFER, logfile);
            qServerLeaf[i][j]->setName("q-server-leaf-" + std::to_string(i) + "-" + std::to_string(j));
            logfile.writeName(*(qServerLeaf[i][j]));

            // createPipe(pipeType, pServerLeaf[i][j], LINK_DELAY);
            pServerLeaf[i][j] = new Pipe(LINK_DELAY);
            pServerLeaf[i][j]->setName("p-server-leaf-" + std::to_string(i) + "-" + std::to_string(j));
            logfile.writeName(*(pServerLeaf[i][j]));
        }
    }

    genFPGA(is_ec_offload, queueType, logfile);
}

// FIXME: for now, usign same params as core-leaf
void CongaTopology::genFPGA(bool is_ec_offload, std::string &queueType, Logfile &logfile) {
    if (is_ec_offload) {
        // connect FPGA to core
        for (int i = 0; i < N_CORE; i++) {
            // Uplink
            createQueue(queueType, qToFPGA[i], CORE_SPEED, LEAF_BUFFER, logfile);
            qToFPGA[i]->setName("q-core-fpga-" + std::to_string(i));
            logfile.writeName(*(qToFPGA[i]));

            pToFPGA[i] = new Pipe(LINK_DELAY);
            pToFPGA[i]->setName("p-core-fpga-" + std::to_string(i));
            logfile.writeName(*(pToFPGA[i]));

            // Downlink
            createQueue(queueType, qFromFPGA[i], CORE_SPEED, CORE_BUFFER, logfile);
            qFromFPGA[i]->setName("q-fpga-core-" + std::to_string(i));
            logfile.writeName(*(qFromFPGA[i]));

            // createPipe(pipeType, pCoreLeaf[i][j], LINK_DELAY);
            pFromFPGA[i] = new Pipe(LINK_DELAY);
            pFromFPGA[i]->setName("p-fpga-core-" + std::to_string(i));
            logfile.writeName(*(pFromFPGA[i]));
        }
    }
    else {
        // TODO: 
        // connect FPGA to srcHost
        // workload fix src traffic from only one server
        // SHOULD NOT USE ANY FPGA QUERUE/PIPE THIS CASE!
        
        // createQueue(queueType, qToFPGA[0], CORE_SPEED, LEAF_BUFFER, logfile);
        // qToFPGA[0]->setName("q-core-fpga-" + std::to_string(0));
        // logfile.writeName(*(qToFPGA[0]));

        // pToFPGA[i] = new Pipe(LINK_DELAY);
        // pToFPGA[i]->setName("p-core-fpga-" + std::to_string(0));
        // logfile.writeName(*(pToFPGA[0]));

        // createQueue(queueType, qFromFPGA[0], CORE_SPEED, CORE_BUFFER, logfile);
        // qFromFPGA[0]->setName("q-fpga-core-" + std::to_string(0));
        // logfile.writeName(*(qFromFPGA[0]));

        // pFromFPGA[0] = new Pipe(LINK_DELAY);
        // pFromFPGA[0]->setName("p-fpga-core-" + std::to_string(0));
        // logfile.writeName(*(pFromFPGA[0]));
    }
    
}

void CongaTopology::createQueue(std::string &qType,
                      Queue *&queue,
                      uint64_t speed,
                      uint64_t buffer,
                      Logfile &logfile)
{
#if MING_PROF
    QueueLoggerSampling *qs = new QueueLoggerSampling(timeFromUs(100));
    //QueueLoggerSampling *qs = new QueueLoggerSampling(timeFromUs(10));
    //QueueLoggerSampling *qs = new QueueLoggerSampling(timeFromUs(50));
#else
    QueueLoggerSampling *qs = new QueueLoggerSampling(timeFromMs(10));
#endif
    logfile.addLogger(*qs);

    if (qType == "fq") {
        queue = new FairQueue(speed, buffer, qs);
    } else if (qType == "afq") {
        queue = new AprxFairQueue(speed, buffer, qs);
    } else if (qType == "pq") {
        queue = new PriorityQueue(speed, buffer, qs);
    } else if (qType == "sfq") {
        queue = new StocFairQueue(speed, buffer, qs);
    } else {
        queue = new Queue(speed, buffer, qs);
    }
}
