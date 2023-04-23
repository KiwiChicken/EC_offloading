/*
 * MPTCP-sim simulator entry point
 */
#include "clock.h"
#include "eventlist.h"
#include "logfile.h"
#include "test.h"

using namespace std;

int parseArgs(int argc, char *argv[], ArgList &args);

void 
printUsage()
{
    cerr << "Usage:" << endl;
    cerr << "./htsim --expt=XX [--<arg1>=<value> --<arg2>=<value> ...]" << endl;
    cerr << endl << "Experiment List" << endl;
    print_experiment_list();
}

int
main(int argc,
     char *argv[])
{
    ArgList args;
    parseArgs(argc, argv, args);

    string logpath = "data/htsim-log";
    if (args.find("logfile") != args.end()) {
        logpath = args["logfile"];
    }

    uint32_t rngSeed = 1729;
    parseInt(args, "rngseed", rngSeed);
    srand(rngSeed);

    uint32_t expt = 0;
    parseInt(args, "expt", expt);
    if (expt == 0) {
        printUsage();
        return 0;
    }

    EventList &eventlist = EventList::Get();
    Logfile logfile(logpath);

    /* Run desired experiment. Complete list defined in <test.h> */
    if (run_experiment(expt, args, logfile)) {
        cerr << "Unknown experiment number\n";
        exit(0);
    }

    // Run the simulation!
    Clock c;
    while (eventlist.doNextEvent()) {}

    cerr << "\nExiting successfully!" << endl;
    return 0;
}

int 
parseArgs(int argc, 
          char *argv[], 
          ArgList &args)
{
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] != '-') {
            cerr << "Ignoring argument (" << i << ") : " << argv[i] << endl;
            continue;
        }

        char *tok = strchr(argv[i], '=');
        string arg = string(argv[i] + 2, tok - argv[i] - 2);
        string val = string(tok + 1);

        cerr << arg << " : " << val << endl;
        args[arg] = val;
    }
    return 0;
}
