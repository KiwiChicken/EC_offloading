#include <vector>
#include "network.h"

#ifndef EC_ROUTE_H_
#define EC_ROUTE_H_

namespace ec_route {
    const int EC_K = 2;
    const int EC_N = 3;

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

    extern double ec_chance;
    extern simtime_picosec ec_exec_time;
    extern uint64_t fpga_mem_limit;
    extern int (*genRoutes)(int, std::vector<flow_info_t>&);
};

#endif