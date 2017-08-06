#include "util.h"

#include <stdarg.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <sstream>

std::string Util::FormatStr(const char* fmt, ...)
{
	#define BUFFER_SIZE 40960
    char temp[BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    int nWrite = vsnprintf(temp, BUFFER_SIZE, fmt, args);

    va_end(args);


    if(nWrite == -1)
    {
        //THROW_EXCEPTION3(0,0, cstring("formatStr overflow, fmt is:")+fmt);
    }
    else if (nWrite >= BUFFER_SIZE)
    {
        //printf("Warning:formatStr overflow,formatStr len %d >= BUFFER_SIZE %d,fmt is:%s",nWrite,BUFFER_SIZE,fmt);
    }
    return std::string(temp);
}

std::string& Util::LTrim(std::string& s)
{
    return s.erase(0, s.find_first_not_of(" \t\n\r"));
}

std::string& Util::RTrim(std::string& s)
{
    return s.erase(s.find_last_not_of(" \t\n\r") + 1);
}

std::string& Util::Trim(std::string& s)
{
    return RTrim(LTrim(s));
}

int Util::IsPathExist(const char* path)
{
    if (access(path, F_OK) < 0) {
        return 0;
    }
    return 1;
}

int Util::IsPathExist(std::string path)
{
    return IsPathExist(path.c_str());
}

std::vector<std::string>& Util::Split(const std::string &s, 
                                      char delim, 
                                      std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

int Util::GetKeyValueWithEqualSign(std::string& str, 
                                   std::string& key, 
                                   std::string& value)
{
    std::string::size_type n;

    key = "";
    value = "";
    if (str.size() == 0) {
        return 0;
    }
    if (str[0] == '#' || str[0] == ';') {
        return 0;
    }

    n = str.find('=');
    if (std::string::npos == n) {
        return 0;
    }
    key = str.substr(0, n);
    value = str.substr(n + 1);
    Trim(key);
    Trim(value);
    if (value.size() == 0) {
        value = " ";
    }
    return 1;
}

#define ADD_ADD_WHITH_SPACE_AFTER_EQUAL 1

int Util::ReplaceValueAfterEqualSign(std::string& str, 
                                     std::string& value)
{
    std::string::size_type n;
    n = str.find('=');
    if (std::string::npos == n) {
        value = "";
        return -1;
    }
#ifdef ADD_ADD_WHITH_SPACE_AFTER_EQUAL
    value.insert(0, " ");
#endif
    str.replace(str.begin() + n + 1 , str.end(), value);
    return 0;
}

// static int 
// SplitByDelimiter(std::string& str, std::string& first, std::string& second, char delimiter)
// {
//     std::string::size_type n;
//     n = str.find(delimiter);
//     if (std::string::npos == n) {
//         first = "";
//         second = "";
//         return -1;
//     }
//     first = str.substr(0, n);
//     second = str.substr(n + 1);
//     return 0;
// }


