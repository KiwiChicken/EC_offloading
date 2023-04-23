/*
 * Experiment test header
 */
#ifndef TESTS_H
#define TESTS_H

#include <string>
#include <unordered_map>

// List of arguments to be passed for an expriment.
typedef std::unordered_map<std::string,std::string> ArgList;

/*
 * List of experiments written for the simulator.
 */
void single_link_simulation(const ArgList &, Logfile &);
void conga_testbed(const ArgList &, Logfile &);
void fat_tree_testbed(const ArgList &, Logfile &);

inline int 
run_experiment(uint32_t expt,
               const ArgList &args,
               Logfile &logfile)
{
    switch(expt) {
        case 1: 
            // Starts multiple finite flows at a target link utilization level.
            single_link_simulation(args, logfile);
            break;

        case 2: 
            // Runs the CONGA testbed and workload.
            conga_testbed(args, logfile);
            break;

        case 3: 
            // Run a fat-tree topology
            fat_tree_testbed(args, logfile);
            break;

        default:
            return -1;
    }
    return 0;
}

inline void 
print_experiment_list()
{
    std::cerr << "  1" << " single_link_simulation" << std::endl;
    std::cerr << "  2" << " conga_testbed" << std::endl;
    std::cerr << "  3" << " fat_tree_testbed" << std::endl;
}

/* Helper functions for parsing arguments. */
inline bool 
parseInt(const ArgList &args, 
         std::string str, 
         uint32_t &val)
{
    if (args.find(str) != args.end()) {
        val = stoul(args.at(str));
        return true;
    }
    return false;
}

inline bool 
parseLongInt(const ArgList &args, 
             std::string str, 
             uint64_t &val)
{
    if (args.find(str) != args.end()) {
        val = stoull(args.at(str));
        return true;
    }
    return false;
}

inline bool 
parseDouble(const ArgList &args, 
            std::string str, 
            double &val)
{
    if (args.find(str) != args.end()) {
        val = stod(args.at(str));
        return true;
    }
    return false;
}

inline bool 
parseString(const ArgList &args, 
            std::string str, 
            std::string &val)
{
    if (args.find(str) != args.end()) {
        val = args.at(str);
        return true;
    }
    return false;
}

#endif /* TESTS_H */
