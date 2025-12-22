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
#include <QClipboard>
#include <QCloseEvent>

// 包含共享配置
#include "myhook/config.h"

// --- 修正点：必须定义在最前面，修复 C2065 EnumWindowsProc 错误 ---
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
    setupTray();      // 初始化托盘
    on_refreshWindows();

    m_logFilePath = "logs/total_history.txt";

    // 初始位置：定位到文件末尾，启动后只增量显示新产生的日志
    QFile file(m_logFilePath);
    if (file.exists()) {
        m_lastPos = file.size(); 
    }

    // 启动扫描定时器 (增量读取磁盘文件)
    m_scanTimer = new QTimer(this);
    connect(m_scanTimer, &QTimer::timeout, this, &MainWindow::updateLogsFromFile);
    m_scanTimer->start(500); 

    // 注册 ScrollLock 热键
    RegisterHotKey((HWND)this->winId(), 2001, 0, VK_SCROLL);
    Config::hMainWnd = (HWND)this->winId();
}

// 核心：彩色日志插入逻辑
void MainWindow::addColoredLog(const LogRecord& rec) {
    QListWidgetItem *item = new QListWidgetItem();
    QString shortTime = rec.time.isValid() ? rec.time.toString("HH:mm:ss") : "--:--:--";
    item->setText(QString("[%1] %2").arg(shortTime).arg(rec.content));

    if (rec.content.contains("Key:")) {
        item->setForeground(QColor("#409eff")); // 蓝色：按键
    } else if (rec.content.contains("Special:")) {
        item->setForeground(QColor("#e6a23c")); // 橙色：功能键
    } else if (rec.content.contains("Click")) {
        item->setForeground(QColor("#f56c6c")); // 红色：点击
    } else {
        item->setForeground(QColor("#909399")); // 灰色：移动
    }

    m_logList->addItem(item);
    m_logList->scrollToBottom();
}

// 核心：扫描并解析日志行
void MainWindow::updateLogsFromFile() {
    QFile file(m_logFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    if (file.size() < m_lastPos) m_lastPos = 0; 
    file.seek(m_lastPos);
    QTextStream in(&file);
    
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.isEmpty()) continue;

        int firstClose = line.indexOf("]");
        int secondOpen = line.indexOf("[", firstClose);
        int secondClose = line.indexOf("]", secondOpen);

        if (firstClose != -1 && secondOpen != -1 && secondClose != -1) {
            LogRecord rec;
            QString timeStr = line.mid(1, firstClose - 1);
            rec.time = QDateTime::fromString(timeStr, "yyyy-MM-dd HH:mm:ss");
            rec.windowName = line.mid(secondOpen + 1, secondClose - secondOpen - 1);
            rec.content = line;
            rec.isKeyboard = line.contains("Key:") || line.contains("Special:");

            m_allLogs.append(rec);
            if (m_allLogs.size() > 10000) m_allLogs.removeFirst();

            if (!m_detectedWindows.contains(rec.windowName)) {
                m_detectedWindows.insert(rec.windowName);
                if (m_winSelector->findText(rec.windowName) == -1) 
                    m_winSelector->addItem(rec.windowName);
            }

            if (checkIfMatchFilter(rec)) {
                addColoredLog(rec);
            }
        }
    }
    m_lastPos = file.pos();
    file.close();
}

// 搜索历史按钮
void MainWindow::on_searchHistory() {
    m_logList->clear();
    m_allLogs.clear();
    m_lastPos = 0; 
    updateLogsFromFile();
    QMessageBox::information(this, "搜索完成", "已根据日期和窗口筛选加载历史记录。");
}

// 复制功能
void MainWindow::on_copyLog() {
    QList<QListWidgetItem*> items = m_logList->selectedItems();
    if (items.isEmpty()) return;
    QStringList texts;
    for (auto item : items) texts << item->text();
    QApplication::clipboard()->setText(texts.join("\n"));
}

void MainWindow::showLogContextMenu(const QPoint &pos) {
    QMenu menu(this);
    QAction *copyAct = menu.addAction("复制选中行内容");
    connect(copyAct, &QAction::triggered, this, &MainWindow::on_copyLog);
    menu.exec(m_logList->mapToGlobal(pos));
}

// 实时同步配置到 DLL（解决录制期间无法直接切换的问题）
void MainWindow::syncConfigToDll() {
    if (!fpSetRecordConfig) return;

    QString target = m_winSelector->currentText();
    const wchar_t* win = (target.contains("全局")) ? L"" : reinterpret_cast<const wchar_t*>(target.utf16());
    
    // 直接通知运行中的 DLL 修改其内存配置
    fpSetRecordConfig(m_checkKbd->isChecked(), m_checkMouse->isChecked(), win);
}

void MainWindow::setupUI() {
    QWidget *central = new QWidget(this);
    QGridLayout *layout = new QGridLayout(central);

    // --- 浅色样式 ---
    this->setStyleSheet(
        "QMainWindow { background-color: #fcfcfc; } "
        "QPushButton { background-color: #ffffff; border: 1px solid #dcdfe6; border-radius: 4px; padding: 7px; color: #606266; } "
        "QPushButton:hover { background-color: #ecf5ff; color: #409eff; } "
        "QListWidget { background-color: #ffffff; border: 1px solid #dcdfe6; font-family: 'Consolas'; font-size: 10pt; } "
        "QDateTimeEdit, QComboBox { background-color: #ffffff; border: 1px solid #dcdfe6; padding: 3px; color: #606266; }"
        "QLabel { color: #909399; font-weight: bold; }"
    );

    m_btnRecord = new QPushButton(" 开始运行监控 (ScrollLock) ");
    m_btnRecord->setFixedHeight(50);
    m_btnRecord->setStyleSheet("font-weight: bold; color: #67c23a; border: 2px solid #67c23a; font-size: 12pt;");
    layout->addWidget(m_btnRecord, 0, 0, 1, 2);

    m_checkKbd = new QCheckBox("记录键盘按键");
    m_checkMouse = new QCheckBox("记录鼠标动作");
    m_checkKbd->setChecked(true); m_checkMouse->setChecked(true);
    layout->addWidget(m_checkKbd, 1, 0);
    layout->addWidget(m_checkMouse, 1, 1);

    m_winSelector = new QComboBox();
    m_winSelector->addItem("--- 全局模式 (所有窗口) ---");
    layout->addWidget(new QLabel("目标窗口过滤:"), 2, 0);
    layout->addWidget(m_winSelector, 2, 1);

    m_startTime = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-1));
    m_endTime = new QDateTimeEdit(QDateTime::currentDateTime().addDays(1));
    m_startTime->setCalendarPopup(true); m_endTime->setCalendarPopup(true);
    layout->addWidget(new QLabel("回溯开始时间:"), 3, 0);
    layout->addWidget(m_startTime, 3, 1);
    layout->addWidget(new QLabel("回溯截止时间:"), 4, 0);
    layout->addWidget(m_endTime, 4, 1);

    m_btnSearch = new QPushButton(" 🔍 搜索并加载满足条件的历史日志 ");
    m_btnSearch->setStyleSheet("background-color: #409eff; color: white; font-weight: bold; height: 35px;");
    layout->addWidget(m_btnSearch, 5, 0, 1, 2);

    m_statusLabel = new QLabel("状态: 空闲");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel, 6, 0, 1, 2);

    m_logList = new QListWidget();
    m_logList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_logList->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_logList, 7, 0, 1, 2);

    setCentralWidget(central);
    setWindowTitle("Input Monitor Pro v4.1");
    resize(900, 750);

    connect(m_btnRecord, &QPushButton::clicked, this, &MainWindow::on_toggleRecording);
    connect(m_btnSearch, &QPushButton::clicked, this, &MainWindow::on_searchHistory);
    connect(m_logList, &QListWidget::customContextMenuRequested, this, &MainWindow::showLogContextMenu);
    
    // 实时更新过滤器
    connect(m_winSelector, &QComboBox::currentTextChanged, this, &MainWindow::applyFilter);
    connect(m_checkKbd, &QCheckBox::stateChanged, this, &MainWindow::applyFilter);
    connect(m_checkMouse, &QCheckBox::stateChanged, this, &MainWindow::applyFilter);

    // 录制期间实时同步 UI 更改到 DLL 配置
    connect(m_checkKbd, &QCheckBox::stateChanged, this, &MainWindow::syncConfigToDll);
    connect(m_checkMouse, &QCheckBox::stateChanged, this, &MainWindow::syncConfigToDll);
    connect(m_winSelector, &QComboBox::currentTextChanged, this, &MainWindow::syncConfigToDll);
}

void MainWindow::setupTray() {
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    
    QMenu *menu = new QMenu(this);
    
    // 关键：在这里将 Action 关联到成员变量 m_actToggle，以便后续修改文字
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

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_trayIcon->isVisible()) {
        this->hide();
        m_trayIcon->showMessage("程序运行中", "已最小化到系统托盘", QSystemTrayIcon::Information, 1000);
        event->ignore(); 
    }
}

void MainWindow::on_toggleRecording() {
    if (!fpInstallHook) return;
    m_isRecording = !m_isRecording;
    
    if (m_isRecording) {
        syncConfigToDll(); // 启动前同步 UI 状态
        fpInstallHook();

        m_btnRecord->setText(" 停止运行监控 (ScrollLock) ");
        m_btnRecord->setStyleSheet("color: #f56c6c; border: 2px solid #f56c6c; font-weight: bold;");
        m_statusLabel->setText("状态: ● 正在监控");
        
        // 关键：同步托盘菜单文字
        if (m_actToggle) m_actToggle->setText("停止录制");
        
        this->hide(); 
    } else {
        fpUninstallHook();
        m_btnRecord->setText(" 开始运行监控 (ScrollLock) ");
        m_btnRecord->setStyleSheet("color: #67c23a; border: 2px solid #67c23a; font-weight: bold;");
        m_statusLabel->setText("状态: 空闲");
        
        // 关键：同步托盘菜单文字
        if (m_actToggle) m_actToggle->setText("开始录制");
        
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
        if (checkIfMatchFilter(log)) addColoredLog(log);
    }
    m_logList->scrollToBottom();
}

void MainWindow::on_refreshWindows() {
    m_winSelector->clear();
    m_winSelector->addItem("--- 全局模式 (所有窗口) ---");
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(m_winSelector));
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY && msg->wParam == 2001) {
        on_toggleRecording();
        return true;
    }
    return QMainWindow::nativeEvent(eventType, message, result);
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