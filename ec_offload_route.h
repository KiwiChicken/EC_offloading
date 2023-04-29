#include <vector>
#include "network.h"
#include "conga-topology.h"

#ifndef EC_OFFLOAD_ROUTE_H_
#define EC_OFFLOAD_ROUTE_H_

namespace ec_offload{
    typedef struct flow_info {
        route_t* fwd_route;
        route_t* rcv_route;
        uint32_t dst;

        flow_info(route_t* fwd, route_t* rcv, uint32_t node_id) {
            fwd_route = fwd;
            rcv_route = rcv;
            dst = node_id;
        }
        
    } flow_info_t;

    int genDstRoutes(std::vector<flow_info_t> &flows);
};

#endif