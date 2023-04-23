/*
 * logfile header
 */
#ifndef LOGFILE_H
#define LOGFILE_H

#include "eventlist.h"

#include <string>

/*
 * Logfile is a class for specifying the log file format.
 * The loggers (loggers.h) face both
 *  1. the log file, using the base class Logger (defined here)
 *  2. the simulator, using the base classes in loggertypes.h
 */

#define MAX_RECORDS 100000

struct __attribute__((__packed__)) Record {
    double   time;
    uint32_t type;
    uint32_t id;
    uint32_t ev;
    double   val1;
    double   val2;
    double   val3;
};

class Logfile;

class Logger
{
    friend class Logfile;
    protected:
    void setLogfile(Logfile &logfile) { _logfile = &logfile; }
    Logfile *_logfile;
};

class Logfile
{
    public:
        Logfile(const std::string &filename,
                simtime_picosec start = 0,
                simtime_picosec end = ULLONG_MAX);
        ~Logfile();

        void addLogger(Logger &logger);
        void write(const std::string &msg);
        void writeName(Logged &logged);
        void writeRecord(uint32_t type, uint32_t id, uint32_t ev,
                double val1, double val2, double val3);

    private:
        FILE *_trace_file;
        FILE *_id_file;

        simtime_picosec _start;
        simtime_picosec _end;

        struct Record *_records;
        uint32_t _nRecords;
        uint32_t _nTotalRecords;
};

#endif /* LOGFILE_H */
