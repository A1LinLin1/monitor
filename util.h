#pragma once
#include <string>

namespace Util {
    std::string getCurrentTimeStr();        // 日志行前缀时间
    std::string getTimestampForFile();      // 文件名用时间戳
    std::string getLocalIPAddress();        // 获取当前主机 IP（用于日志命名）
}


