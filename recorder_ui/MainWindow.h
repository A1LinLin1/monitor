#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <QList>
#include <QSet>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include <windows.h>

// 日志结构体
struct LogRecord {
    QDateTime time;
    QString windowName;
    QString content;
    bool isKeyboard;
};

// DLL 接口指针
typedef void (*InstallFunc)();
typedef void (*UninstallFunc)();
typedef void (*SetConfigFunc)(bool, bool, const wchar_t*);

class QPushButton;
class QCheckBox;
class QComboBox;
class QListWidget;
class QDateTimeEdit;
class QLabel;
class QAction;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    // 拦截 Windows 原生消息 (WM_COPYDATA)
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    // 拦截关闭事件实现托盘化
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_toggleRecording();   // 开始/停止录制
    void on_refreshWindows();    // 刷新窗口列表
    void applyFilter();          // 执行日志过滤显示
    void on_trayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void setupUI();
    void setupTray();
    void loadHookDLL();
    void loadHistory();          // 加载 total_history.txt
    bool checkIfMatchFilter(const LogRecord& log);

private:
    // UI 核心组件
    QPushButton *m_btnRecord;
    QCheckBox *m_checkKbd;
    QCheckBox *m_checkMouse;
    QComboBox *m_winSelector;
    QDateTimeEdit *m_startTime;
    QDateTimeEdit *m_endTime;
    QLabel *m_statusLabel;
    QListWidget *m_logList;

    // 托盘与菜单项
    QSystemTrayIcon *m_trayIcon;
    QAction *m_actToggle; // 托盘：开始/停止 切换
    QAction *m_actKbd;    // 托盘：同步键盘勾选
    QAction *m_actMouse;  // 托盘：同步鼠标勾选

    // 内部数据
    QList<LogRecord> m_allLogs;
    QSet<QString> m_detectedWindows;
    bool m_isRecording;

    // DLL 导出函数
    InstallFunc fpInstallHook = nullptr;
    UninstallFunc fpUninstallHook = nullptr;
    SetConfigFunc fpSetRecordConfig = nullptr;
};

#endif