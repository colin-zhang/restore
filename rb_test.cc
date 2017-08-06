#include "logger_ring_buffer.h"

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <sched.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <string>

#define gettid_syscall() syscall(__NR_gettid)


struct LogInfo
{
    std::string name;
    uint32_t index;
    uint32_t timestamp;
};

struct ThreadInfo
{
    pthread_t tid;
    int core_num;
    int run;
};

static pthread_mutex_t g_lock;
static pthread_cond_t  g_cond;

static volatile int g_running = 1;
static volatile int g_waiting = 1;

static uint32_t g_idx = 0;
RingBuff<LogInfo>* g_log_ring;

static void
pthreadCall(const char* label, int result) {
    if (result != 0) {
        fprintf(stderr, "pthread %s: %s\n", label, strerror(result));
        abort();
    }
}

static void* push_ring(void* arg)
{
    cpu_set_t mask;
    int core = (int)(*(int*)arg);

    printf("thread (pid:%d)(tid:%ld)(thread id:%ld) will run at core id=%d \n", 
            getpid(), syscall(__NR_gettid), pthread_self(), core);

    CPU_ZERO(&mask);
    CPU_SET(core, &mask);
    if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
        fprintf(stderr, "set thread affinity failed\n");
        pthread_exit(NULL);
    }

    LogInfo log_info;
    int ret = 0;
    while (g_running) {
        pthreadCall("g_lock lock", pthread_mutex_lock(&g_lock));
        g_idx++;
        log_info.index = g_idx;
        pthreadCall("g_lock unlock", pthread_mutex_unlock(&g_lock));

        log_info.timestamp = 0;
        ret = g_log_ring->Put(&log_info);
        if (ret != 0) {
            printf("break, g_idx = %u\n", g_idx);
            //break;
            sleep(2);
            g_running = 0;
        }
        usleep(5);
    }
    pthread_exit(NULL);
    return NULL;
}

static void signal_handler(int sig) {
     printf("SIGINT, g_idx = %u\n", g_idx);
     exit(0);
}

int main(int argc, char const *argv[])
{
    int i, flag;
    signal(SIGINT, signal_handler);
    int num = sysconf(_SC_NPROCESSORS_CONF);
    struct ThreadInfo* thread_info = NULL;

    uint32_t size = 1 << 10;
    uint32_t fail_cnt = 0;
    uint32_t time_cnt = 0;
    g_log_ring = new RingBuff<LogInfo>(size);

    LogInfo* log_buf = new LogInfo[size];

    thread_info = new ThreadInfo[num];

    pthreadCall("g_lock init", pthread_mutex_init(&g_lock, NULL));
    //pthreadCall("g_cond init", pthread_cond_init(&g_cond, NULL));

    for (i = 8; i < 16; i++) {
        thread_info[i].core_num = i;
        thread_info[i].run = 0;
        if (pthread_create(&thread_info[i].tid, NULL, 
                           (void* (*)(void*))push_ring, 
                           (void*)&thread_info[i].core_num) != 0) {
            fprintf(stderr, "thread create failed\n");
            break;
        }
        thread_info[i].run = 1;
    }

    while (1) {
        //pthreadCall("g_lock lock", pthread_mutex_lock(&g_lock));
        //pthreadCall("g_lock unlock", pthread_mutex_unlock(&g_lock));
        //pthreadCall("g_cond broadcast", pthread_cond_broadcast(&g_cond));
        if (!g_running) {
            g_log_ring->Print();
            break;
        }

        if (g_log_ring->GetHalf(log_buf) == 0)
        {
            for (uint32_t i = 0; i < (size >> 1); i++) {
                // /printf("i = %u idx = %u\n", i, log_buf[i].index);
            }
            fail_cnt = 0;
            //break;
        } else {
            fail_cnt++;
            if (fail_cnt > 10000) {
                //printf("GetHalf fail\n");
                //g_log_ring->Print();
                fail_cnt = 0;
                time_cnt++;
                if (time_cnt > 10000) {
                    //break;
                }
            }

        }
    }

    for (i = 0; i < num; i++) {
        if (thread_info[i].run) {
            pthread_join(thread_info[i].tid, NULL);
        }
    }
    pthreadCall("g_lock destryoy", pthread_mutex_destroy(&g_lock));
    pthreadCall("g_cond destryoy", pthread_cond_destroy(&g_cond));
    free(thread_info);
    return 0;
}
