#include "conga-topology.h"

void CongaTopology::genTopology(std::string &queueType, Logfile &logfile) {
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
            pCoreLeaf[i][j]->setName("p-leaf-core-" + std::to_string(i) + "-" + std::to_string(j));
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
