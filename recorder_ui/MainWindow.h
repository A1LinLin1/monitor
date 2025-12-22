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
    void applyFilter();
    void updateLogsFromFile(); // 定时扫描函数
    void on_trayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void setupUI();
    void setupTray();
    void loadHookDLL();
    void loadHistory(); 
    void addColoredLog(const LogRecord& rec); // 酷炫彩色日志
    bool checkIfMatchFilter(const LogRecord& log);

private:
    QPushButton *m_btnRecord;
    QCheckBox *m_checkKbd;
    QCheckBox *m_checkMouse;
    QComboBox *m_winSelector;
    QDateTimeEdit *m_startTime;
    QDateTimeEdit *m_endTime;
    QLabel *m_statusLabel;
    QListWidget *m_logList;

    QSystemTrayIcon *m_trayIcon;
    QAction *m_actKbd;
    QAction *m_actMouse;
    QAction *m_actToggle;

    QList<LogRecord> m_allLogs;
    QSet<QString> m_detectedWindows;
    bool m_isRecording;
    
    // 文件扫描相关
    QString m_logFilePath;
    qint64 m_lastPos;
    QTimer *m_scanTimer;

    InstallFunc fpInstallHook = nullptr;
    UninstallFunc fpUninstallHook = nullptr;
    SetConfigFunc fpSetRecordConfig = nullptr;
};

#endif