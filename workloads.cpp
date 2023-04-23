/*
 * Workloads
 */
#include "workloads.h"

using namespace std;

Workloads::Workloads(uint32_t avgFlowSize, 
                     FlowDist flowSizeDist)
                    : _avgFlowSize(avgFlowSize),
                    _flowSizeDist(flowSizeDist)
{
    if (_flowSizeDist == ENTERPRISE) {
        _avgFlowSize = 215000;
        for (uint32_t i = 0; i < sizeof(enterprise_size)/8; i++) {
            _flowSizeCDF[enterprise_prob[i]] = enterprise_size[i];
        }
    } else if (_flowSizeDist == DATAMINING) {
        _avgFlowSize = 12500000;
        for (uint32_t i = 0; i < sizeof(datamining_size)/8; i++) {
            _flowSizeCDF[datamining_prob[i]] = datamining_size[i];
        }
    }
}

uint64_t
Workloads::generateFlowSize()
{
    switch (_flowSizeDist) {
        case PARETO:
            // Pareto
            return (uint64_t)pareto(1.1, _avgFlowSize);

        case ENTERPRISE:
        case DATAMINING:
            // Custom workload, generate using _flowSizeCDF.
            break;

        default: // UNIFORM
            return _avgFlowSize;
    }

    double random = drand();
    auto it = _flowSizeCDF.upper_bound(random);
    double rp = it->first;
    uint64_t rv = it->second;
    it = prev(it);
    double lp = it->first;
    uint64_t lv = it->second;

    uint64_t flowsize = lv + (rv - lv) * (random - lp) / (rp - lp);
    return flowsize;
}
