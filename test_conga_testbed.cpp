#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "aprx-fairqueue.h"
#include "fairqueue.h"
#include "priorityqueue.h"
#include "stoc-fairqueue.h"
#include "flow-generator.h"
#include "pipe.h"
#include "test.h"

namespace conga {

    // tesdbed configuration
    const int N_CORE = 12;
    const int N_LEAF = 24;
    const int N_SERVER = 32;   // Per leaf

    const uint64_t LEAF_BUFFER = 512000;
    const uint64_t CORE_BUFFER = 1024000;
    const uint64_t ENDH_BUFFER = 8192000;

    const uint64_t LEAF_SPEED = 10000000000; // 10gbps
    const uint64_t CORE_SPEED = 40000000000; // 40gbps

    
}

using namespace std;
using namespace conga;

void
conga_testbed(const ArgList &args, Logfile &logfile)
{
    // testbed definition
}
