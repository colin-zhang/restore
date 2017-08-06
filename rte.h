#ifndef RET_H_
#define RET_H_ 

#include "define.h"
#include <string>

class Rte
{
public:
    Rte();
    ~Rte();

    static long GetPid();
private:
    DISALLOW_COPY_AND_ASSIGN(Rte);

public:
    long core_num;
    int  packet_core_num;
    int  logger_core_num;
    bool is_gzip;
    std::string pcap_file;
};

extern Rte GlobalRte;

#endif