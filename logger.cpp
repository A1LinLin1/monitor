#include "logger.h"
#include "util.h"
#include <filesystem>

Logger::Logger(const std::string& folder) : stopThread(false) {
    if (!std::filesystem::exists(folder)) {
        std::filesystem::create_directory(folder);
    }
    std::string filename = folder + "/" + Util::getTimestampForFile() + ".txt";
    outFile.open(filename, std::ios::out | std::ios::app);

    // 启动异步写入线程
    workerThread = std::thread(&Logger::processLogs, this);
}

Logger::~Logger() {
    stopThread = true;
    cv.notify_all();
    if (workerThread.joinable()) {
        workerThread.join();
    }
    if (outFile.is_open()) {
        outFile.close();
    }
}

void Logger::log(const std::string& message) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        // 钩子线程只负责丢入队列，不等待写入，防止卡顿
        logQueue.push("[" + Util::getCurrentTimeStr() + "] " + message);
    }
    cv.notify_one();
}

void Logger::processLogs() {
    while (true) {
        std::string entry;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [this] { return !logQueue.empty() || stopThread; });

            if (stopThread && logQueue.empty()) break;

            entry = logQueue.front();
            logQueue.pop();
        }

        if (outFile.is_open()) {
            outFile << entry << std::endl; // 只有后台线程执行 IO 操作
        }
    }
}