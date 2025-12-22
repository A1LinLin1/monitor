#include "MainWindow.h"
#include <QVBoxLayout>

// ... 构造函数初始化 UI ...

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_COPYDATA) {
        PCOPYDATASTRUCT pcds = reinterpret_cast<PCOPYDATASTRUCT>(msg->lParam);
        QString rawData = QString::fromUtf8((char*)pcds->lpData);

        // 解析日志 (假设 DLL 发送格式为 "[窗口名] 内容")
        // 注意：建议修改 DLL 直接发送结构化数据，或者在这里用正则解析
        LogEntry entry;
        entry.timestamp = QDateTime::currentDateTime();
        
        int endIdx = rawData.indexOf("]");
        if (endIdx > 0) {
            entry.windowTitle = rawData.mid(1, endIdx - 1);
            entry.content = rawData.mid(endIdx + 1).trimmed();
        }

        // 1. 添加到数据池
        allLogs.append(entry);

        // 2. 动态更新窗口筛选列表
        if (!recordedWindows.contains(entry.windowTitle)) {
            recordedWindows.insert(entry.windowTitle);
            windowFilterCombo->addItem(entry.windowTitle); // 自动增加可选窗口
        }

        // 3. 如果符合当前筛选条件，实时显示
        on_filterChanged(); 
        return true;
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::on_filterChanged() {
    QString targetWin = windowFilterCombo->currentText();
    QDateTime start = startTimeEdit->dateTime();
    QDateTime end = endTimeEdit->dateTime();

    logListWidget->clear();
    for (const auto& log : allLogs) {
        // 时间和窗口双重判定
        bool timeMatch = (log.timestamp >= start && log.timestamp <= end);
        bool winMatch = (targetWin == "--- All Windows ---" || log.windowTitle == targetWin);

        if (timeMatch && winMatch) {
            QString displayStr = QString("[%1] (%2) %3")
                                    .arg(log.timestamp.toString("HH:mm:ss"))
                                    .arg(log.windowTitle)
                                    .arg(log.content);
            logListWidget->addItem(displayStr);
        }
    }
    logListWidget->scrollToBottom();
}