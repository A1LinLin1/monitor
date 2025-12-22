#pragma once
#include <string>
#include <fstream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

class Logger
{
public:
    Logger(const std::string &folder = "logs");
    ~Logger();
    void log(const std::string &message);

private:
    void processLogs(); // 后台写入线程函数

    std::ofstream outFile;
    std::queue<std::string> logQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    std::thread workerThread;
    std::atomic<bool> stopThread;
};