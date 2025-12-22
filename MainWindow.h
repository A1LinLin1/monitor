#include <QMainWindow>
#include <QDateTime>
#include <QList>
#include <QSet>
#include <windows.h>

// 定义日志条目结构
struct LogEntry {
    QDateTime timestamp;
    QString windowTitle;
    QString content;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    // 关键：接管原生 Windows 消息以接收 DLL 的 WM_COPYDATA
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private slots:
    void on_filterChanged(); // 任何筛选条件改变时触发
    void on_recordBtnClicked();

private:
    void updateLogDisplay(); // 刷新显示的列表
    
    // UI 元素 (建议使用 Qt Designer 设计)
    // QComboBox *windowFilterCombo;
    // QDateTimeEdit *startTimeEdit, *endTimeEdit;
    // QListWidget *logListWidget;

    QList<LogEntry> allLogs;      // 存储所有原始数据
    QSet<QString> recordedWindows; // 存储所有记录过的窗口标题
    bool isRecording = false;
};