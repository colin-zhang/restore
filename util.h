#ifndef UTIL_H_
#define UTIL_H_ 

#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include <string>
#include <vector>

class Util
{
public:
    static std::string FormatStr(const char* fmt, ...);
    static std::string& LTrim(std::string& s);
    static std::string& RTrim(std::string& s);
    static std::string& Trim(std::string& s);
    static int IsPathExist(const char* path);
    static int IsPathExist(std::string path);
    static std::vector<std::string>& Split(const std::string &s, 
                                    char delim, 
                                    std::vector<std::string> &elems);

    static int GetKeyValueWithEqualSign(std::string& str, 
                                        std::string& key, 
                                        std::string& value);

    static int ReplaceValueAfterEqualSign(std::string& str, 
                                     std::string& value);



};


static inline long get_micros() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return static_cast<long>(tv.tv_sec) * 1000000 + tv.tv_usec;
}

class AutoTimer {
public:
    AutoTimer(double timeout_ms = -1, const char* msg1 = NULL, const char* msg2 = NULL)
      : timeout_(timeout_ms),
        msg1_(msg1),
        msg2_(msg2) {
        start_ = get_micros();
    }
    int64_t TimeUsed() const {
        return get_micros() - start_;
    }
    ~AutoTimer() {
        fprintf(stderr, "%s\n", "~AutoTimer");
        if (timeout_ == -1) return;
        long end = get_micros();
        if (end - start_ > timeout_ * 1000) {
            double t = (end - start_) / 1000.0;
            if (!msg2_) {
                fprintf(stderr, "[AutoTimer] %s use %.3f ms\n",
                    msg1_, t);
                fflush(stderr);
            } else {
                fprintf(stderr, "[AutoTimer] %s %s use %.3f ms\n",
                    msg1_, msg2_, t);
                fflush(stderr);
            }
        }
    }
private:
    long start_;
    double timeout_;
    const char* msg1_;
    const char* msg2_;
};


#endif