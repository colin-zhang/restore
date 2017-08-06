#include "rte.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <string>

#include "util.h"

#ifndef __NR_gettid
#define __NR_gettid SYS_gettid
#endif

#define gettid_syscall() syscall(__NR_gettid)

Rte GlobalRte;

Rte::Rte()
    : core_num(sysconf(_SC_NPROCESSORS_CONF)),
      packet_core_num(8),
      logger_core_num(1),
      is_gzip(0),
      pcap_file("./test.pcap")

{
    char buf[1024] = {0};
    FILE* f = fopen(".config", "r");
    if (nullptr == f) {
        return;
    }
    while (fgets(buf, sizeof(buf), f)) {
        std::string line = std::string(buf);
        std::string key, value;
        Util::Trim(line);
        if (Util::GetKeyValueWithEqualSign(line, key, value)) {
            if (key == "packet_core_num") {
                packet_core_num = atoi(value.c_str());
            } else if (key == "logger_core_num") {
                logger_core_num = atoi(value.c_str());
            } else if (key == "is_gzip") {
                if (value == "true" || value == "TRUE") {
                    is_gzip = true;
                } else {
                    is_gzip = false;
                }
            } else if (key == "pcap_file") {
                pcap_file = value;
            }
        }

    }   
    fclose(f);
}


Rte::~Rte()
{
    
}

long Rte::GetPid() 
{
    return gettid_syscall();
}