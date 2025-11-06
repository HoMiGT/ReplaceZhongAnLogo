#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <qfileinfo.h>
#include <QMainWindow>
#include <QString>
#include <QLineEdit>
#include <QRunnable>
#include <QVector>
#include <QSet>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

enum class FlipKind
{
    Unchanged,
    LeftRight,
    UpDown,
    Mirror
};

struct ReplaceParams
{
    QFileInfoList imgInfos;
    QString logoPath;
    QString saveDir;
    FlipKind kind;
    ReplaceParams(const QFileInfoList &imgs, const QString &logo, const QString &save, FlipKind fk = FlipKind::Unchanged)
        : imgInfos(imgs), logoPath(logo), saveDir(save), kind(fk) {}
};

struct ReplaceResult
{
    bool state;
    int totalCount;
    int successCount;
    QString errorMsg;
};


class ReplaceTask: public QObject, public QRunnable
{
    Q_OBJECT
public:
    ReplaceTask(ReplaceParams &&params);
    void run() override;

signals:
    void task_status_update(ReplaceResult rr);

private:
    ReplaceParams m_params;
    QString m_error;

    bool replace_logo(const QString& logoPath,const QString& saveDir, const QFileInfo &img, const FlipKind kind) noexcept;
};


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pb_img_path_clicked();

    void on_pb_save_path_clicked();

    void on_pb_start_clicked();

    void on_pb_close_clicked();

    void on_pb_logo_path_clicked();

    void on_task_status_update(ReplaceResult rr);

private:
    Ui::MainWindow *ui;
    QVector<ReplaceTask*> m_tasks;
    QSet<QString> m_taskErrors;
    int m_taskFinished{0};
    int m_taskTotalCount{0};
    int m_taskSuccessCount{0};
    int m_idealCount{1};
    QString m_version;
    QString m_logoPath;
    QString m_imgDir;
    QString m_saveDir;

    void setStyle();
    void updateRequiredState(QLineEdit* le);
    bool validateRequiredFields();
};
#endif // MAINWINDOW_H
