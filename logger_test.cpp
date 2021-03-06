#include <signal.h>

#include <iostream>
#include <map>
#include <string>
#include <chrono>
#include <mutex>
#include <functional>
#include <vector>

#include "thread.h"
#include "define.h"
#include "pcap.h"

#include "ring_buffer.h"
#include "buffer_ring.h"
#include "access_cmdline.h"
#include "rte.h"
#include "logger.h"

#include "clock_time.h"

class LoggerManager
{
public:
    LoggerManager(int size) 
      :  size_(size)
    {
        logger_.reserve(size_);
    }
    ~LoggerManager() 
    {

    }

private:
    std::vector<BasicBusinessLogger*> logger_;
    int size_;
};

static bool StopRunning = false;
static bool SkipOutput = false;

static PcapPacket gPacketBuff[16 << 10];

static std::vector<Thread*> gThreads;
static PcapReader* gPcapReaderPtr = nullptr;

static BasicBusinessLogger gLogger;

static void signal_handler(int sig) 
{
    printf("StopRunning\n\n");
    StopRunning = true;
}


static void PacketGet(ThreadOption& opt)
{
    printf("%s %d started\n", opt.name.c_str(), opt.id);
    PcapPacketVector ppv = gPcapReaderPtr->GetPcapPacketVector(opt.id);
    printf("ppv.size = %lu \n", ppv.size());

    ClockTime clock_time;
    uint64_t packet_size = ppv.size();

    uint32_t kTimes = 100;
    uint32_t cnt = 0;

    clock_time.GatherNow();
    while (1) {
        if (unlikely(StopRunning)) {
            break;
        }

        if (unlikely(cnt == kTimes)) {
            break;
        }

        for (auto p : ppv) {
            gLogger.push_back(&p);
            //Pause();
        }
        cnt++;
    }
    double us = clock_time.PrintDuration();
    double rate = (double)packet_size * kTimes / us;
    printf("%s %d exited!, %f / us\n", opt.name.c_str(), opt.id, rate);
}

static void LoggerWrite(ThreadOption& opt)
{
    printf("%s %d started\n", opt.name.c_str(), opt.id);

    while (1) {
        if (unlikely(StopRunning)) {
            break;
        }

        if (unlikely(SkipOutput)) {
            usleep(1);
            continue;
        }

        if (gLogger.checkRotate()) {
            //break;
        }
        usleep(1);
    }
}

void PcapReaderInit()
{
    gPcapReaderPtr = new PcapReader(GlobalRte.packet_core_num);
    gPcapReaderPtr->ReadPcapFile(GlobalRte.pcap_file.c_str());
}

void PcapReaderDestory()
{
    delete gPcapReaderPtr;
}


static void CmdLineProcess(ThreadOption& opt)
{
    sleep(1);
    CmdLine cmd_line("logger test > ", ".test_history");
    while (1) {
        if (unlikely(StopRunning)) {
            break;
        }

        std::string cmd = cmd_line.readLine();
        if (cmd.size() > 0) {
            if (cmd == "stop") {
                StopRunning = true;
                break;
            } else if (cmd == "core_num") {
                printf("%ld \n", GlobalRte.core_num);
            } else if (cmd == "skip_output") {
                SkipOutput = true;
            } else if (cmd == "no_skip_output") {
                SkipOutput = false;
            }
            printf("logger test > ");
            fflush(stdout);
        }
    }
}


void ThreadInit() 
{
    for (int i = 0; i < GlobalRte.logger_core_num; i++) {
        Thread* thd = new Thread(LoggerWrite);
        thd->Option.name = "logger_thread";
        thd->Option.id = i;
        thd->Option.cores.push_back(i + 1 + GlobalRte.packet_core_num);
        gThreads.push_back(thd);
    }

    for (int i = 0; i < GlobalRte.packet_core_num; i++) {
        Thread* thd = new Thread(PacketGet);
        thd->Option.name = "packet_thread";
        thd->Option.id = i;
        thd->Option.cores.push_back(i + 1);
        gThreads.push_back(thd);
    }

    for (auto th : gThreads) {
        th->Start();
    }
}

void ThreadDestory()
{
    for (auto th : gThreads) {
        th->Join();
        delete th;
    }
}

void Init()
{
    signal(SIGINT, signal_handler);
    PcapReaderInit();
    gLogger.init("./log", 100 << 20, 1, GlobalRte.is_gzip ? kCompressGzip : kCompressNone);
    ThreadInit();

    printf("sizeof(PcapPacket) = %u \n", sizeof(PcapPacket));
}

int main() 
{
    Init();
    Thread cmd_thd(CmdLineProcess);
    cmd_thd.Start();
    cmd_thd.Join();
    ThreadDestory();
    PcapReaderDestory();
    return 0;
}



