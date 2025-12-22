#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <QList>
#include <QSet>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include <windows.h>

struct LogRecord {
    QDateTime time;
    QString windowName;
    QString content;
    bool isKeyboard;
};

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
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_toggleRecording();
    void on_refreshWindows();
    void on_searchHistory();      // 搜索历史按钮
    void applyFilter();           // 过滤当前内存数据
    void updateLogsFromFile();    // 定时扫描增量文件
    void on_copyLog();            // 复制选中日志
    void syncConfigToDll();
    void on_trayActivated(QSystemTrayIcon::ActivationReason reason);
    void showLogContextMenu(const QPoint &pos); // 日志右键菜单

private:
    void setupUI();
    void setupTray();
    void loadHookDLL();
    void addColoredLog(const LogRecord& rec); 
    bool checkIfMatchFilter(const LogRecord& log);

private:
    // UI
    QPushButton *m_btnRecord;
    QPushButton *m_btnSearch;
    QCheckBox *m_checkKbd;
    QCheckBox *m_checkMouse;
    QComboBox *m_winSelector;
    QDateTimeEdit *m_startTime;
    QDateTimeEdit *m_endTime;
    QLabel *m_statusLabel;
    QListWidget *m_logList;

    // 托盘
    QSystemTrayIcon *m_trayIcon;
    QAction *m_actToggle;
    QAction *m_actKbd;   
    QAction *m_actMouse;

    // 数据与状态
    QList<LogRecord> m_allLogs;
    QSet<QString> m_detectedWindows;
    bool m_isRecording;
    
    // 扫描逻辑变量
    QString m_logFilePath;
    qint64 m_lastPos;
    QTimer *m_scanTimer;

    // DLL 接口
    InstallFunc fpInstallHook = nullptr;
    UninstallFunc fpUninstallHook = nullptr;
    SetConfigFunc fpSetRecordConfig = nullptr;
};

#endif