#ifndef THREAD_H_
#define THREAD_H_

#include <string>
#include <vector>
#include <thread>
#include <functional>

#include <pthread.h>

#include "define.h"

struct ThreadOption
{
    std::vector<int> cores;
    std::string name;
    int id;
    void* arg;

    ThreadOption()  
      : arg(nullptr) { }
};

class Thread
{

typedef std::function<void(ThreadOption&)> ThreadFun;

public:
    Thread(ThreadFun fun) : fun_(fun) {
    }
    ~Thread() {
        delete thread_;
    }
    void Start() {
        thread_ = new std::thread(fun_, std::ref(Option));
        SetAffinity();
    }
    void Join() {
        thread_->join();
    }

private:
    void SetAffinity() {
    #if 0
        if (Option.cores.size() == 0) return;
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for (auto c : Option.cores) {
            CPU_SET(c, &cpuset);
        }
        int rc = pthread_setaffinity_np(thread_->native_handle(), sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            fprintf(stderr, "%s\n", "pthread_setaffinity_np error");
            exit(-1);
        }
    #endif
    }
public:
    ThreadOption Option;
private:
    ThreadFun fun_;
    DISALLOW_COPY_AND_ASSIGN(Thread);
    std::thread* thread_;
};


#endif
