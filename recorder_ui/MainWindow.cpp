#include "MainWindow.h"
#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QStyle>
#include <QDateTimeEdit>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QTimer>

// 包含共享配置
#include "myhook/config.h"

// Win32 窗口枚举回调函数
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    wchar_t title[256];
    if (IsWindowVisible(hwnd) && GetWindowText(hwnd, title, 256) > 0) {
        QComboBox* combo = reinterpret_cast<QComboBox*>(lParam);
        QString t = QString::fromWCharArray(title);
        if (combo->findText(t) == -1) combo->addItem(t);
    }
    return TRUE;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_isRecording(false), m_lastPos(0) {
    loadHookDLL();
    setupUI();
    setupTray();
    on_refreshWindows();

    // 1. 确定日志路径（确保与 logger.cpp 中一致）
    m_logFilePath = "logs/total_history.txt";

    // 2. 初始化读取位置：跳过历史，从当前时间点开始增量显示
    QFile file(m_logFilePath);
    if (file.exists()) {
        m_lastPos = file.size(); 
    }

    // 3. 启动扫描定时器 (每500ms检查一次文件是否有新行)
    m_scanTimer = new QTimer(this);
    connect(m_scanTimer, &QTimer::timeout, this, &MainWindow::updateLogsFromFile);
    m_scanTimer->start(500);

    // 绑定热键 ScrollLock
    RegisterHotKey((HWND)this->winId(), 2001, 0, VK_SCROLL);
    Config::hMainWnd = (HWND)this->winId();
}

// 核心：增量读取文件并着色显示
void MainWindow::updateLogsFromFile() {
    QFile file(m_logFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    // 如果文件大小变小了（说明被清理过），重置指针
    if (file.size() < m_lastPos) m_lastPos = 0;
    
    file.seek(m_lastPos);
    QTextStream in(&file);
    
    bool hasNewData = false;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.isEmpty()) continue;

        // 解析格式: [2025-12-23 01:00:00] [窗口名] 内容
        int firstClose = line.indexOf("]");
        int secondOpen = line.indexOf("[", firstClose);
        int secondClose = line.indexOf("]", secondOpen);

        if (firstClose != -1 && secondOpen != -1 && secondClose != -1) {
            LogRecord rec;
            // 提取时间
            QString timeStr = line.mid(1, firstClose - 1);
            rec.time = QDateTime::fromString(timeStr, "yyyy-MM-dd HH:mm:ss");
            // 提取窗口名
            rec.windowName = line.mid(secondOpen + 1, secondClose - secondOpen - 1);
            rec.content = line;
            rec.isKeyboard = line.contains("Key:") || line.contains("Special:");

            // 存入内存缓存用于筛选
            m_allLogs.append(rec);
            if (m_allLogs.size() > 5000) m_allLogs.removeFirst();

            // 动态发现新窗口
            if (!m_detectedWindows.contains(rec.windowName)) {
                m_detectedWindows.insert(rec.windowName);
                if (m_winSelector->findText(rec.windowName) == -1) 
                    m_winSelector->addItem(rec.windowName);
            }

            // 如果符合当前筛选条件，插入彩色日志
            if (checkIfMatchFilter(rec)) {
                addColoredLog(rec);
                hasNewData = true;
            }
        }
    }
    m_lastPos = file.pos();
    file.close();

    if (hasNewData) m_logList->scrollToBottom();
}

// 浅色主题下的日志着色
void MainWindow::addColoredLog(const LogRecord& rec) {
    QListWidgetItem *item = new QListWidgetItem();
    // 界面只显示 [时:分:秒] + 内容，更简洁
    QString shortTime = rec.time.isValid() ? rec.time.toString("HH:mm:ss") : QTime::currentTime().toString("HH:mm:ss");
    item->setText(QString("[%1] %2").arg(shortTime).arg(rec.content));

    // 根据内容类型设置深色系颜色（确保在白色背景上清晰）
    if (rec.content.contains("Key:")) {
        item->setForeground(QColor("#0055cc")); // 深蓝色：普通按键
    } else if (rec.content.contains("Special:")) {
        item->setForeground(QColor("#880088")); // 深紫色：功能键 (Ctrl/Alt等)
    } else if (rec.content.contains("Click")) {
        item->setForeground(QColor("#d35400")); // 深橙色：点击
    } else {
        item->setForeground(QColor("#7f8c8d")); // 灰色：移动
    }
    m_logList->addItem(item);
}

void MainWindow::setupUI() {
    QWidget *central = new QWidget(this);
    QGridLayout *layout = new QGridLayout(central);

    // --- 浅色清新皮肤 QSS ---
    this->setStyleSheet(
        "QMainWindow { background-color: #f5f6f7; } "
        "QPushButton { background-color: #ffffff; border: 1px solid #dcdfe6; border-radius: 4px; padding: 8px; color: #606266; } "
        "QPushButton:hover { background-color: #ecf5ff; color: #409eff; border-color: #c6e2ff; } "
        "QListWidget { background-color: #ffffff; border: 1px solid #dcdfe6; border-radius: 4px; font-family: 'Consolas', 'Microsoft YaHei'; font-size: 10pt; } "
        "QCheckBox { color: #606266; } "
        "QComboBox, QDateTimeEdit { background-color: #ffffff; border: 1px solid #dcdfe6; padding: 3px; color: #606266; } "
        "QLabel { color: #909399; font-size: 9pt; }"
    );

    // 控制按钮
    m_btnRecord = new QPushButton(" 开始录制 (ScrollLock) ");
    m_btnRecord->setFixedHeight(50);
    m_btnRecord->setStyleSheet("font-weight: bold; font-size: 12pt; color: #2ecc71; border: 2px solid #2ecc71;");
    layout->addWidget(m_btnRecord, 0, 0, 1, 2);

    // 勾选框
    m_checkKbd = new QCheckBox("记录键盘按键");
    m_checkMouse = new QCheckBox("记录鼠标操作");
    m_checkKbd->setChecked(true); m_checkMouse->setChecked(true);
    layout->addWidget(m_checkKbd, 1, 0);
    layout->addWidget(m_checkMouse, 1, 1);

    // 筛选
    m_winSelector = new QComboBox();
    m_winSelector->addItem("--- 全局模式 (所有窗口) ---");
    QPushButton *btnRefresh = new QPushButton("刷新列表");
    layout->addWidget(new QLabel("目标窗口:"), 2, 0);
    layout->addWidget(m_winSelector, 3, 0);
    layout->addWidget(btnRefresh, 3, 1);

    // 时间筛选
    m_startTime = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-1));
    m_endTime = new QDateTimeEdit(QDateTime::currentDateTime().addDays(1));
    m_startTime->setCalendarPopup(true); m_endTime->setCalendarPopup(true);
    layout->addWidget(new QLabel("历史回溯 (从):"), 4, 0);
    layout->addWidget(m_startTime, 5, 0);
    layout->addWidget(new QLabel("至 (到):"), 4, 1);
    layout->addWidget(m_endTime, 5, 1);

    m_statusLabel = new QLabel("状态: 等绪");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel, 6, 0, 1, 2);

    // 日志列表
    m_logList = new QListWidget();
    layout->addWidget(m_logList, 7, 0, 1, 2);

    setCentralWidget(central);
    setWindowTitle("Input Monitor Pro v3.8");
    resize(850, 750);

    // 信号绑定
    connect(m_btnRecord, &QPushButton::clicked, this, &MainWindow::on_toggleRecording);
    connect(btnRefresh, &QPushButton::clicked, this, &MainWindow::on_refreshWindows);
    connect(m_winSelector, &QComboBox::currentTextChanged, this, &MainWindow::applyFilter);
    connect(m_checkKbd, &QCheckBox::stateChanged, this, &MainWindow::applyFilter);
    connect(m_checkMouse, &QCheckBox::stateChanged, this, &MainWindow::applyFilter);
}

void MainWindow::setupTray() {
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    
    QMenu *menu = new QMenu(this);
    m_actToggle = menu->addAction("开始录制", this, &MainWindow::on_toggleRecording);
    menu->addSeparator();

    m_actKbd = menu->addAction("记录键盘");
    m_actKbd->setCheckable(true); m_actKbd->setChecked(true);
    connect(m_actKbd, &QAction::triggered, m_checkKbd, &QCheckBox::setChecked);
    connect(m_checkKbd, &QCheckBox::toggled, m_actKbd, &QAction::setChecked);

    m_actMouse = menu->addAction("记录鼠标");
    m_actMouse->setCheckable(true); m_actMouse->setChecked(true);
    connect(m_actMouse, &QAction::triggered, m_checkMouse, &QCheckBox::setChecked);
    connect(m_checkMouse, &QCheckBox::toggled, m_actMouse, &QAction::setChecked);

    menu->addSeparator();
    menu->addAction("恢复窗口", this, &MainWindow::showNormal);
    menu->addAction("彻底退出", qApp, &QApplication::quit);

    m_trayIcon->setContextMenu(menu);
    m_trayIcon->show();
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::on_trayActivated);
}

void MainWindow::on_toggleRecording() {
    if (!fpInstallHook) {
        QMessageBox::warning(this, "错误", "DLL 未加载，请检查 myhook.dll");
        return;
    }

    m_isRecording = !m_isRecording;
    if (m_isRecording) {
        QString target = m_winSelector->currentText();
        const wchar_t* win = (target.contains("全局")) ? L"" : reinterpret_cast<const wchar_t*>(target.utf16());
        
        fpSetRecordConfig(m_checkKbd->isChecked(), m_checkMouse->isChecked(), win);
        fpInstallHook();

        m_btnRecord->setText(" 停止录制 (ScrollLock) ");
        m_btnRecord->setStyleSheet("font-weight: bold; font-size: 12pt; color: #e74c3c; border: 2px solid #e74c3c;");
        m_statusLabel->setText("状态: ● 正在监控");
        m_actToggle->setText("停止录制");
        this->hide(); 
    } else {
        fpUninstallHook();
        m_btnRecord->setText(" 开始录制 (ScrollLock) ");
        m_btnRecord->setStyleSheet("font-weight: bold; font-size: 12pt; color: #2ecc71; border: 2px solid #2ecc71;");
        m_statusLabel->setText("状态: 等绪");
        m_actToggle->setText("开始录制");
        this->showNormal();
    }
}

bool MainWindow::checkIfMatchFilter(const LogRecord& log) {
    if (log.time.isValid() && (log.time < m_startTime->dateTime() || log.time > m_endTime->dateTime())) return false;
    
    QString targetWin = m_winSelector->currentText();
    if (targetWin != "--- 全局模式 (所有窗口) ---" && log.windowName != targetWin) return false;

    if (log.isKeyboard && !m_checkKbd->isChecked()) return false;
    if (!log.isKeyboard && !m_checkMouse->isChecked()) return false;

    return true;
}

void MainWindow::applyFilter() {
    m_logList->clear();
    for (const auto& log : m_allLogs) {
        if (checkIfMatchFilter(log)) {
            addColoredLog(log);
        }
    }
    m_logList->scrollToBottom();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_trayIcon->isVisible()) {
        this->hide();
        event->ignore();
    }
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY && msg->wParam == 2001) {
        on_toggleRecording();
        return true;
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::on_refreshWindows() {
    m_winSelector->clear();
    m_winSelector->addItem("--- 全局模式 (所有窗口) ---");
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(m_winSelector));
}

void MainWindow::loadHookDLL() {
    HMODULE hDll = GetModuleHandle(L"myhook.dll");
    if (!hDll) hDll = LoadLibrary(L"myhook.dll");
    if (hDll) {
        fpInstallHook = (InstallFunc)GetProcAddress(hDll, "InstallHook");
        fpUninstallHook = (UninstallFunc)GetProcAddress(hDll, "UninstallHook");
        fpSetRecordConfig = (SetConfigFunc)GetProcAddress(hDll, "SetRecordConfig");
    }
}

MainWindow::~MainWindow() {
    UnregisterHotKey((HWND)this->winId(), 2001);
}

void MainWindow::on_trayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
        this->showNormal();
        this->activateWindow();
    }
}