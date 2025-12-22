#include "logger.h"
#include "util.h"
#include "config.h"
#include <filesystem>
#include <windows.h>

Logger::Logger(const std::string& folder) : stopThread(false) {
    if (!std::filesystem::exists(folder)) {
        std::filesystem::create_directory(folder);
    }
    // 固定的历史文件名
    std::string filename = folder + "/total_history.txt";
    outFile.open(filename, std::ios::out | std::ios::app);
    workerThread = std::thread(&Logger::processLogs, this);
}

Logger::~Logger() {
    stopThread = true;
    cv.notify_all();
    if (workerThread.joinable()) workerThread.join();
    if (outFile.is_open()) outFile.close();
}

void Logger::log(const std::string& message) {
    std::string fullMsg = "[" + Util::getCurrentTimeStr() + "] " + message;
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        logQueue.push(fullMsg);
    }
    cv.notify_one();

    if (Config::hMainWnd && IsWindow(Config::hMainWnd)) {
        COPYDATASTRUCT cds;
        cds.dwData = 1; 
        cds.cbData = (DWORD)fullMsg.length() + 1;
        cds.lpData = (PVOID)fullMsg.c_str();
        
        DWORD_PTR dwResult;
        SendMessageTimeout(Config::hMainWnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds, 
                           SMTO_ABORTIFHUNG, 200, &dwResult);
    }
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
        if (outFile.is_open()) outFile << entry << std::endl;
    }
}