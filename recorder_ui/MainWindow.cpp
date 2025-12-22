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

#include "myhook/config.h"

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    wchar_t title[256];
    if (IsWindowVisible(hwnd) && GetWindowText(hwnd, title, 256) > 0) {
        QComboBox* combo = reinterpret_cast<QComboBox*>(lParam);
        QString t = QString::fromWCharArray(title);
        if (combo->findText(t) == -1) combo->addItem(t);
    }
    return TRUE;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_isRecording(false) {
    loadHookDLL();
    setupUI();
    setupTray();
    on_refreshWindows();
    loadHistory(); 

    RegisterHotKey((HWND)this->winId(), 2001, 0, VK_SCROLL);
    Config::hMainWnd = (HWND)this->winId();
}

void MainWindow::loadHistory() {
    QFile file("logs/total_history.txt");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.isEmpty()) continue;

        int firstClose = line.indexOf("]");
        int secondOpen = line.indexOf("[", firstClose);
        int secondClose = line.indexOf("]", secondOpen);

        if (firstClose != -1 && secondOpen != -1 && secondClose != -1) {
            LogRecord rec;
            rec.time = QDateTime::fromString(line.mid(1, firstClose - 1), "yyyy-MM-dd HH:mm:ss");
            rec.windowName = line.mid(secondOpen + 1, secondClose - secondOpen - 1);
            rec.content = line;
            rec.isKeyboard = line.contains("Key:") || line.contains("Special:");
            m_allLogs.append(rec);
            m_detectedWindows.insert(rec.windowName);
        }
    }
    file.close();
    for(const QString& win : m_detectedWindows) {
        if(m_winSelector->findText(win) == -1) m_winSelector->addItem(win);
    }
    applyFilter();
}

void MainWindow::setupUI() {
    QWidget *central = new QWidget(this);
    QGridLayout *layout = new QGridLayout(central);

    m_btnRecord = new QPushButton(" 开始录制 (ScrollLock) ");
    m_btnRecord->setFixedHeight(50);
    layout->addWidget(m_btnRecord, 0, 0, 1, 2);

    m_checkKbd = new QCheckBox("记录键盘按键");
    m_checkMouse = new QCheckBox("记录鼠标相对位移");
    m_checkKbd->setChecked(true);
    m_checkMouse->setChecked(true);
    layout->addWidget(m_checkKbd, 1, 0);
    layout->addWidget(m_checkMouse, 1, 1);

    m_winSelector = new QComboBox();
    m_winSelector->addItem("--- 全局模式 (所有窗口) ---");
    QPushButton *btnRefresh = new QPushButton(" 刷新列表 ");
    layout->addWidget(new QLabel("目标窗口:"), 2, 0);
    layout->addWidget(m_winSelector, 3, 0);
    layout->addWidget(btnRefresh, 3, 1);

    m_startTime = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-7));
    m_endTime = new QDateTimeEdit(QDateTime::currentDateTime().addDays(1));
    m_startTime->setCalendarPopup(true);
    m_endTime->setCalendarPopup(true);
    layout->addWidget(new QLabel("历史回溯筛选 (从):"), 4, 0);
    layout->addWidget(m_startTime, 5, 0);
    layout->addWidget(new QLabel("至 (到):"), 4, 1);
    layout->addWidget(m_endTime, 5, 1);

    m_statusLabel = new QLabel("状态: 空闲");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel, 6, 0, 1, 2);

    m_logList = new QListWidget();
    m_logList->setStyleSheet("font-family: 'Consolas'; font-size: 10pt;");
    layout->addWidget(m_logList, 7, 0, 1, 2);

    setCentralWidget(central);
    setWindowTitle("Pro Input Recorder v3.6");
    resize(900, 750);

    connect(m_btnRecord, &QPushButton::clicked, this, &MainWindow::on_toggleRecording);
    connect(btnRefresh, &QPushButton::clicked, this, &MainWindow::on_refreshWindows);
    connect(m_winSelector, &QComboBox::currentTextChanged, this, &MainWindow::applyFilter);
    connect(m_startTime, &QDateTimeEdit::dateTimeChanged, this, &MainWindow::applyFilter);
    connect(m_endTime, &QDateTimeEdit::dateTimeChanged, this, &MainWindow::applyFilter);
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
    menu->addAction("恢复界面", this, &MainWindow::showNormal);
    menu->addAction("彻底退出", qApp, &QApplication::quit);

    m_trayIcon->setContextMenu(menu);
    m_trayIcon->show();
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::on_trayActivated);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_trayIcon->isVisible()) {
        this->hide();
        m_trayIcon->showMessage("Monitor Recorder", "程序已最小化到托盘", QSystemTrayIcon::Information, 1500);
        event->ignore(); 
    }
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY && msg->wParam == 2001) {
        on_toggleRecording();
        return true;
    }
    if (msg->message == WM_COPYDATA) {
        PCOPYDATASTRUCT pcds = reinterpret_cast<PCOPYDATASTRUCT>(msg->lParam);
        if (!pcds || !pcds->lpData) return true;

        QString raw = QString::fromUtf8(reinterpret_cast<const char*>(pcds->lpData));

        int firstClose = raw.indexOf("]");
        int secondOpen = raw.indexOf("[", firstClose);
        int secondClose = raw.indexOf("]", secondOpen);

        if (secondOpen != -1 && secondClose != -1) {
            LogRecord rec;
            rec.time = QDateTime::currentDateTime();
            rec.content = raw;
            rec.windowName = raw.mid(secondOpen + 1, secondClose - secondOpen - 1);
            rec.isKeyboard = raw.contains("Key:") || raw.contains("Special:");
            
            m_allLogs.append(rec);
            if (m_allLogs.size() > 5000) m_allLogs.removeFirst();
            
            if (!m_detectedWindows.contains(rec.windowName)) {
                m_detectedWindows.insert(rec.windowName);
                if (m_winSelector->findText(rec.windowName) == -1) 
                    m_winSelector->addItem(rec.windowName);
            }

            if (checkIfMatchFilter(rec)) {
                m_logList->addItem(QString("[%1] %2").arg(rec.time.toString("HH:mm:ss")).arg(rec.content));
                m_logList->scrollToBottom();
            }
        }
        return true;
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

bool MainWindow::checkIfMatchFilter(const LogRecord& log) {
    if (log.time < m_startTime->dateTime() || log.time > m_endTime->dateTime()) return false;
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
            m_logList->addItem(QString("[%1] %2").arg(log.time.toString("HH:mm:ss")).arg(log.content));
        }
    }
    m_logList->scrollToBottom();
}

void MainWindow::on_toggleRecording() {
    if (!fpInstallHook) {
        QMessageBox::critical(this, "错误", "无法加载 myhook.dll 导出函数！");
        return;
    }

    m_isRecording = !m_isRecording;
    if (m_isRecording) {
        QString target = m_winSelector->currentText();
        const wchar_t* win = (target.contains("全局")) ? L"" : reinterpret_cast<const wchar_t*>(target.utf16());
        
        fpSetRecordConfig(m_checkKbd->isChecked(), m_checkMouse->isChecked(), win);
        fpInstallHook();

        m_btnRecord->setText(" 停止录制 (ScrollLock) ");
        m_statusLabel->setText("状态: ● 正在录制");
        m_statusLabel->setStyleSheet("color: red; font-weight: bold;");
        m_actToggle->setText("停止录制");
        this->hide(); 
    } else {
        fpUninstallHook();
        m_btnRecord->setText(" 开始录制 (ScrollLock) ");
        m_statusLabel->setText("状态: 空闲");
        m_statusLabel->setStyleSheet("color: black; font-weight: normal;");
        m_actToggle->setText("开始录制");
        this->showNormal();
        this->activateWindow();
    }
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