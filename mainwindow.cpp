#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <format>
#include <QFileDialog>
#include <QThreadPool>
#include <QDir>
#include <QMessageBox>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <vector>
#include "convertEncodingFormat.h"
// #include <QDebug>

ReplaceTask::ReplaceTask(ReplaceParams &&params)
    :m_params(std::move(params))
{
    setAutoDelete(true);
}

void ReplaceTask::run()
{
    const auto& logoPath = m_params.logoPath;
    const auto& saveDir = m_params.saveDir;
    const auto& kind = m_params.kind;
    int count{0};
    for (const auto& img: m_params.imgInfos)
    {
        if (const auto ret = replace_logo(logoPath,saveDir, img, kind); ret){
            count++;
        }else
        {
            // qDebug() << "replace_logo failed : " << ret<<"  error: "<<m_error;
        }
    }
    const int total = static_cast<int>(m_params.imgInfos.size());
    emit task_status_update(ReplaceResult{total == count, total,count,m_error});
}

bool ReplaceTask::replace_logo(const QString& logoPath,
                               const QString& saveDir,
                               const QFileInfo &img,
                               const FlipKind kind) noexcept
{
    try
    {
        const auto &name = img.baseName();
        const auto saveName = QString("%1/%2.png").arg(saveDir, name);
        auto absImgPath = img.absoluteFilePath();
        std::string absStdImgPath{};
        std::string saveStdName{};
        if (const auto ret = IsUtf8(); ret)
        {
            absStdImgPath = absImgPath.toStdString();
            saveStdName = saveName.toStdString();
        }else
        {
            bool convertState{};
            absStdImgPath = WideToLocalACP(absImgPath.toStdWString(),convertState);
            if (!convertState)
            {
                m_error = QString("图片路径编码转换失败: %1").arg(absImgPath);
                return false;
            }
            saveStdName = WideToLocalACP(saveName.toStdWString(),convertState);
            if (!convertState)
            {
                m_error = QString("保存路径编码转换失败: %1").arg(saveName);
                return false;
            }
        }
        // 读取底图
        cv::Mat background = cv::imread(absStdImgPath, cv::IMREAD_UNCHANGED);
        if (background.empty())
        {
            m_error = QString("无法读取底图: %1").arg(QString::fromStdString(absStdImgPath));
            return false;
        }

        // 读取 logo（必须带 alpha）
        cv::Mat logo = cv::imread(logoPath.toStdString(), cv::IMREAD_UNCHANGED);
        if (logo.empty())
        {
            m_error = QString("无法读取Logo图片: %1").arg(logoPath);
            return false;
        }

        // 统一：底图转成 4 通道 BGRA
        if (background.channels() == 3)
        {
            cv::cvtColor(background, background, cv::COLOR_BGR2BGRA);
        }
        else if (background.channels() == 1)
        {
            cv::cvtColor(background, background, cv::COLOR_GRAY2BGRA);
        }
        else if (background.channels() != 4)
        {
            m_error = QString("不支持的底图通道数: %1").arg(background.channels());
            return false;
        }

        // 统一：logo 也转成 4 通道 BGRA
        if (logo.channels() == 3)
        {
            cv::cvtColor(logo, logo, cv::COLOR_BGR2BGRA);
        }
        else if (logo.channels() == 4)
        {
            // ok
        }
        else
        {
            m_error = QString("Logo通道数必须为3或4，目前为: %1").arg(logo.channels());
            return false;
        }

        // 尺寸检查
        if (logo.cols > background.cols || logo.rows > background.rows)
        {
            m_error = QString("Logo尺寸大于背景图尺寸: %1").arg(img.baseName());
            return false;
        }

        // 计算居中位置
        int x = (background.cols - logo.cols) / 2;
        int y = (background.rows - logo.rows) / 2;

        // 取 ROI
        cv::Rect roiRect(x, y, logo.cols, logo.rows);
        cv::Mat roi = background(roiRect);

        // 逐像素 alpha 混合
        for (int row = 0; row < logo.rows; ++row)
        {
            const cv::Vec4b* logoPtr = logo.ptr<cv::Vec4b>(row);
            cv::Vec4b* roiPtr = roi.ptr<cv::Vec4b>(row);

            for (int col = 0; col < logo.cols; ++col)
            {
                const cv::Vec4b &fg = logoPtr[col]; // 前景：logo
                cv::Vec4b &bg = roiPtr[col];        // 背景：底图 ROI

                unsigned char alpha_fg = fg[3];
                if (alpha_fg == 0)
                {
                    // 完全透明，直接跳过
                    continue;
                }

                float a = alpha_fg / 255.0f; // 前景 alpha ∈ [0,1]

                // 混合 BGR 三个通道
                for (int c = 0; c < 3; ++c)
                {
                    float cf = fg[c]; // 前景颜色
                    float cb = bg[c]; // 背景颜色
                    float out = cb * (1.0f - a) + cf * a;
                    bg[c] = static_cast<uchar>(std::round(out));
                }
                // alpha 通道简单取较大值（你也可以按更严格公式计算）
                bg[3] = std::max(bg[3], alpha_fg);
            }
        }

        if (kind == FlipKind::LeftRight)
        {
            cv::flip(background, background, 1);
        }
        else if (kind == FlipKind::UpDown)
        {
            cv::flip(background, background, 0);
        }
        else if (kind == FlipKind::Mirror)
        {
            cv::flip(background, background, -1);
        }

        // 保存图像
        if (!cv::imwrite(saveStdName, background))
        {
            m_error = QString("无法保存图片: %1").arg(saveName);
            return false;
        }

        return true;
    }
    catch (const std::exception &e)
    {
        m_error = QString("图片: %1, 替换Logo异常: %2").arg(img.baseName()).arg(e.what());
        return false;
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // 设置样式
    setStyle();

    // 初始化线程池
    m_idealCount = QThread::idealThreadCount();
    QThreadPool::globalInstance()->setMaxThreadCount(m_idealCount);

    // 设置状态栏信息
    m_version = QString::fromStdString(std::format(" ©{} 杭州沃朴物联科技有限公司 版权所有  版本: 1.0.0", QDate::currentDate().year()));
    ui->statusbar->setSizeGripEnabled(false);
    ui->statusbar->showMessage(m_version);

    // 默认值
    ui->le_logo_path->setText("./logo.png");

    // 测试
    // ui->le_img_path->setText(R"(E:\Projects\PyProjects\ReplaceLogo\2511_orgImgs)");
    // ui->le_save_path->setText(R"(E:\Projects\PyProjects\ReplaceLogo\2511_logoImgs)");

    updateRequiredState(ui->le_logo_path);
    updateRequiredState(ui->le_img_path);
    updateRequiredState(ui->le_save_path);
    connect(ui->le_logo_path, &QLineEdit::textChanged,this,[this](const QString&)
    {
        updateRequiredState(ui->le_logo_path);
    });
    connect(ui->le_img_path,&QLineEdit::textChanged, this, [this](const QString&)
    {
        updateRequiredState(ui->le_img_path);
    });
    connect(ui->le_save_path, &QLineEdit::textChanged, this, [this](const QString&)
    {
        updateRequiredState(ui->le_save_path);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setStyle()
{
    this->setStyleSheet(R"(
/* 整个窗口背景白色 */
QWidget {
    font-family: 'Segoe UI', '微软雅黑', Arial, sans-serif;
    font-size: 17px;
    color: #2e2e2e;
    background: #fff;
}

QLabel.title, QLabel#title {
    font-size: 24px;
    font-weight: 600;
    color: #2196f3;
    letter-spacing: 0.5px;
}

QLabel {
    font-size: 16px;
    color: #1a1a1a;
}

/* 输入框带悬浮高亮 */
QLineEdit, QTextEdit {
    background: #fafbfc;
    border: 2px solid #e3e8ee;
    border-radius: 10px;
    padding: 8px 14px;
    color: #23272f;
    font-size: 16px;
}
QLineEdit:focus, QTextEdit:focus {
    border: 2px solid #2196f3;
    background: #f0f6ff;
}

/* 按钮带悬浮阴影与变色 */
QPushButton {
    background-color: #2196f3;
    color: white;
    border: none;
    border-radius: 8px;
    font-size: 16px;
    font-weight: 500;
    padding: 2px 4px;
    margin: 2px;
}
QPushButton:hover {
    background-color: #1565c0;
}
QPushButton:pressed {
    background-color: #1976d2;
}

/* 进度条现代悬浮感 */
QProgressBar {
    min-height: 32px;
    background: #ecf5fe;
    border-radius: 16px;
    text-align: center;
    font-size: 15px;
    color: #2196f3;
    border: none;
}
QProgressBar::chunk {
    background: qlineargradient(
        x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #2196f3, stop: 1 #41b983
    );
    border-radius: 16px;
}
QRadioButton{
    font-size:14px;
    font-weight: 300;
}

)");


    ui->statusbar->setStyleSheet(R"(
QStatusBar {
    font-size: 12px;            /* 字体小一点 */
    color: #9e9e9e;             /* 颜色浅一点 */
    background: transparent;    /* 背景透明 */
    padding: 4px 8px;           /* 内边距 */
}
QStatusBar QLabel {
    color: #9e9e9e;
    font-size: 12px;
}
)");
}


void MainWindow::updateRequiredState(QLineEdit* le)
{
    if (!le) return;
    const bool empty = le->text().trimmed().isEmpty();
    if (empty)
    {
        le->setStyleSheet("border: 2px solid #e53935; background: #fff7f7; border-radius:6px;");
        le->setToolTip("此项为必填");
    }else
    {
        le->setStyleSheet(R"(
QLineEdit, QTextEdit {
    background: #fafbfc;
    border: 2px solid #e3e8ee;
    border-radius: 10px;
    padding: 4px 8px;
    color: #23272f;
    font-size: 16px;
}
QLineEdit:focus, QTextEdit:focus {
    border: 2px solid #2196f3;
    background: #f0f6ff;
}
)"); // 恢复到默认样式（或设置你期望的正常样式）
        le->setToolTip(QString());
    }

}

bool MainWindow::validateRequiredFields()
{
    bool ok = true;
    if (ui->le_img_path->text().trimmed().isEmpty()) {
        updateRequiredState(ui->le_img_path);
        ok = false;
    }
    if (ui->le_save_path->text().trimmed().isEmpty()) {
        updateRequiredState(ui->le_save_path);
        ok = false;
    }

    if (!ok) {
        ui->statusbar->showMessage("请填写所有必填项", 5000); // 5 秒后自动清除
    }
    return ok;
}

void MainWindow::on_pb_logo_path_clicked()
{
    const QString filter = "PNG 文件 (*.png)";
    const QString innerPath = m_logoPath.isEmpty()? QDir::homePath() : m_logoPath;
    const QString path = QFileDialog::getOpenFileName(this,"选择 PNG 文件", innerPath,filter);
    if (path.trimmed().isEmpty()){return;}
    m_logoPath = QFileInfo(path).absoluteFilePath();
    ui->le_logo_path->setText(path);
}



void MainWindow::on_pb_img_path_clicked()
{
    const QString innerPath = m_imgDir.isEmpty()?QDir::homePath(): m_imgDir;
    const auto path = QFileDialog::getExistingDirectory(this,"选择图片路径",innerPath,QFileDialog::ShowDirsOnly);
    if (path.trimmed().isEmpty()){return;}
    m_imgDir = path;
    ui->le_img_path->setText(path);
}


void MainWindow::on_pb_save_path_clicked()
{
    const QString innerPath = m_saveDir.isEmpty()?QDir::homePath(): m_saveDir;
    const auto path = QFileDialog::getExistingDirectory(this,"选择保存路径",innerPath,QFileDialog::ShowDirsOnly);
    if (path.trimmed().isEmpty()){return;}
    m_saveDir = path;
    ui->le_save_path->setText(path);
}

static QVector<QFileInfoList> splitFileInfoList(const QFileInfoList &files, int n, bool roundRobin=false)
{
    QVector<QFileInfoList> parts;
    if (n <= 0) return parts;
    parts.resize(n);

    const int total = files.size();
    if (total == 0) return parts;

    if (roundRobin) {
        for (int i = 0; i < total; ++i) {
            parts[i % n].append(files.at(i));
        }
    } else {
        int base = total / n;
        int rem = total % n; // 前 rem 份各多 1 个
        int idx = 0;
        for (int p = 0; p < n; ++p) {
            int cnt = base + (p < rem ? 1 : 0);
            for (int k = 0; k < cnt && idx < total; ++k) {
                parts[p].append(files.at(idx++));
            }
        }
    }
    return parts;
}


void MainWindow::on_pb_start_clicked()
{
    FlipKind fk;
    if (ui->rb_unchanged->isChecked())
    {
        fk = FlipKind::Unchanged;
    }else if (ui->rb_leftRight->isChecked())
    {
        fk = FlipKind::LeftRight;
    }else if (ui->rb_upDown->isChecked())
    {
        fk = FlipKind::UpDown;
    }else if (ui->rb_mirror->isChecked())
    {
        fk = FlipKind::Mirror;
    }else
    {
        fk = FlipKind::Unchanged;
    }

    m_logoPath = ui->le_logo_path->text();
    if (m_logoPath.trimmed().isEmpty())
    {
        QMessageBox::warning(this,"警告","请设置 Logo 图片路径");
        return;
    }
    m_imgDir = ui->le_img_path->text();
    if (m_imgDir.trimmed().isEmpty())
    {
        QMessageBox::warning(this,"警告","请设置图片路径");
        return;
    }
    m_saveDir = ui->le_save_path->text();
    if (m_saveDir.trimmed().isEmpty())
    {
        QMessageBox::warning(this,"警告","请设置保存路径");
        return;
    }
    const QDir imgDir(m_imgDir);
    if (!imgDir.exists())
    {
        const auto result = QString("图片路径不存在: %1").arg(m_imgDir);
        QMessageBox::warning(this,"警告",result);
        return;
    }
    m_tasks.clear();
    m_taskErrors.clear();
    const QStringList filters = {"*.png","*.jpg","*.jpeg","*.bmp","*.gif"};
    auto const files = imgDir.entryInfoList(filters, QDir::Files);
    m_taskTotalCount = static_cast<int>(files.size());
    m_taskSuccessCount = 0;

    ui->progressBar->setMinimum(0);
    ui->progressBar->setMaximum(100);
    ui->progressBar->setValue(0);
    ui->progressBar->show();

    QVector<QFileInfoList> tasks;
    auto parts = splitFileInfoList(files, m_idealCount);
    for (const auto& part: parts)
    {
        m_tasks.append(new ReplaceTask(ReplaceParams(
            part, m_logoPath, m_saveDir,fk)));
    }
    const int totalTasks = m_tasks.size();

    m_taskFinished = 0;
    for (auto task : m_tasks)
    {
        connect(task, &ReplaceTask::task_status_update, this, [this,task,totalTasks](ReplaceResult rr)
        {
            this->on_task_status_update(std::move(rr));
            ++m_taskFinished;
            // auto it = std::find(m_tasks.begin(), m_tasks.end(), task);
            // if (it != m_tasks.end())
            // {
            //     m_tasks.erase(it);
            // }
            task->setAutoDelete(true);
            if (m_taskFinished == totalTasks)
            {
                if (!m_taskErrors.isEmpty())
                {
                    const auto errors = m_taskErrors.values();
                    const auto msg = QString("任务已完成!\n共 %1 张, 成功替换 %2 张\n\n出现以下错误:\n%3")
                          .arg(m_taskTotalCount)
                          .arg(m_taskSuccessCount)
                          .arg(errors.join("\n"));
                    QMessageBox::warning(this, "任务完成 - 错误汇总", msg);
                }else
                {
                    ui->progressBar->setValue(100);
                    ui->progressBar->show();
                    QMessageBox::information(this,"提示",QString("任务已完成!\n共 %1 张,成功替换 %2 张").arg(m_taskTotalCount).arg(m_taskSuccessCount));
                }
            }
        },Qt::QueuedConnection);
        QThreadPool::globalInstance()->start(task);
    }
}

void MainWindow::on_task_status_update(ReplaceResult rr)
{
    if (!rr.state)
    {
        m_taskErrors.insert(rr.errorMsg);
    }
    m_taskSuccessCount += rr.successCount;
    const int progress = static_cast<int>((static_cast<double>(m_taskSuccessCount) / m_taskTotalCount) * 100);
    ui->progressBar->setValue(progress);
}

void MainWindow::on_pb_close_clicked()
{
    this->close();
}



