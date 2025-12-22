// myhook/util.cpp
#include "util.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace Util {

std::string getCurrentTimeStr() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    struct tm localTime;
    localtime_s(&localTime, &now_c);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &localTime);
    return std::string(buf);
}

std::string getTimestampForFile() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    struct tm localTime;
    localtime_s(&localTime, &now_c);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d_%H%M%S", &localTime);
    return std::string(buf);
}

std::string getLocalIPAddress() {
    char ac[80];
    if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR)
        return "unknown";

    struct hostent* phe = gethostbyname(ac);
    if (phe == 0) return "unknown";

    for (int i = 0; phe->h_addr_list[i] != 0; ++i) {
        struct in_addr addr;
        memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
        return inet_ntoa(addr);  // 只返回第一个
    }
    return "unknown";
}
}
