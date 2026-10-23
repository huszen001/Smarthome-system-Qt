#include "record_window.h"
#include "qcustomplot.h"
#include <QDateTime>
#include <QVector>
#include <QMargins>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QDateEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QGroupBox>

RecordWindow::RecordWindow(const QString& username, QWidget *parent)
    : QDialog(parent), m_username(username)
{
    setWindowTitle("智能家居数据中心");
    setFixedSize(1000, 700);
    setWindowModality(Qt::ApplicationModal);
    initUI();
    initChart();
    onQueryOperationRecords();
    onQueryEnvData();
}

void RecordWindow::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // ========== 操作记录查询区域 ==========
    QGroupBox *opGroup = new QGroupBox("操作记录查询", this);
    QGridLayout *opGrid = new QGridLayout(opGroup);
    opGrid->setSpacing(10);

    QLabel *deviceTypeLabel = new QLabel("设备类型：", this);
    m_deviceTypeCombo = new QComboBox(this);
    m_deviceTypeCombo->addItems({"全部", "灯", "空调", "窗帘", "摄像头"});

    QLabel *operationTypeLabel = new QLabel("操作类型：", this);
    m_operationTypeCombo = new QComboBox(this);
    m_operationTypeCombo->addItems({"全部", "开启", "关闭", "调节"});

    QLabel *startDateLabel = new QLabel("开始日期：", this);
    m_startDateEdit = new QDateEdit(QDate::currentDate().addDays(-7), this);
    m_startDateEdit->setCalendarPopup(true);
    m_startDateEdit->setDisplayFormat("yyyy-MM-dd");

    QLabel *endDateLabel = new QLabel("结束日期：", this);
    m_endDateEdit = new QDateEdit(QDate::currentDate(), this);
    m_endDateEdit->setCalendarPopup(true);
    m_endDateEdit->setDisplayFormat("yyyy-MM-dd");

    QPushButton *queryOpBtn = new QPushButton("查询记录", this);
    QPushButton *exportOpBtn = new QPushButton("导出记录", this);

    opGrid->addWidget(deviceTypeLabel, 0, 0);
    opGrid->addWidget(m_deviceTypeCombo, 0, 1);
    opGrid->addWidget(operationTypeLabel, 0, 2);
    opGrid->addWidget(m_operationTypeCombo, 0, 3);
    opGrid->addWidget(startDateLabel, 0, 4);
    opGrid->addWidget(m_startDateEdit, 0, 5);
    opGrid->addWidget(endDateLabel, 0, 6);
    opGrid->addWidget(m_endDateEdit, 0, 7);
    opGrid->addWidget(queryOpBtn, 0, 8);
    opGrid->addWidget(exportOpBtn, 0, 9);

    m_opTable = new QTableWidget(this);
    m_opTable->setColumnCount(5);
    m_opTable->setHorizontalHeaderLabels({"操作人", "设备名称", "操作内容", "操作结果", "操作时间"});
    m_opTable->horizontalHeader()->setStretchLastSection(true);
    m_opTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    opGrid->addWidget(m_opTable, 1, 0, 1, 10);
    mainLayout->addWidget(opGroup);

    // ========== 环境数据查询区域 ==========
    QGroupBox *envGroup = new QGroupBox("环境数据查询", this);
    QHBoxLayout *envHLayout = new QHBoxLayout(envGroup);

    m_envTable = new QTableWidget(this);
    m_envTable->setColumnCount(4);
    m_envTable->setHorizontalHeaderLabels({"记录时间", "温度(℃)", "湿度(%)", "空气质量(%)"});
    m_envTable->horizontalHeader()->setStretchLastSection(true);
    m_envTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_envTable->setFixedHeight(150);
    envHLayout->addWidget(m_envTable, 1);

    m_envChart = new QCustomPlot(this);
    m_envChart->setFixedWidth(400);
    m_envChart->setMinimumHeight(300);
    envHLayout->addWidget(m_envChart, 1);
    mainLayout->addWidget(envGroup);

    QPushButton *exportEnvBtn = new QPushButton("导出环境数据", this);
    mainLayout->addWidget(exportEnvBtn, 0, Qt::AlignRight);

    QPushButton *backBtn = new QPushButton("返回主页", this);
    backBtn->setStyleSheet("QPushButton { background-color: #3498db; color: white; border-radius: 5px; padding: 10px 20px; font-size: 14px; }"
                           "QPushButton:hover { background-color: #2980b9; }");
    mainLayout->addWidget(backBtn, 0, Qt::AlignCenter);

    connect(queryOpBtn, &QPushButton::clicked, this, &RecordWindow::onQueryOperationRecords);
    connect(exportOpBtn, &QPushButton::clicked, this, &RecordWindow::onExportOperationRecords);
    connect(exportEnvBtn, &QPushButton::clicked, this, &RecordWindow::onExportEnvData);
    connect(backBtn, &QPushButton::clicked, this, &RecordWindow::close);
}

void RecordWindow::initChart()
{
    if (!m_envChart) return;

    m_envChart->addGraph();
    m_envChart->addGraph();
    m_envChart->addGraph();

    m_envChart->graph(0)->setPen(QPen(Qt::red, 2));
    m_envChart->graph(1)->setPen(QPen(Qt::blue, 2));
    m_envChart->graph(2)->setPen(QPen(Qt::green, 2));
    m_envChart->graph(0)->setName("温度(℃)");
    m_envChart->graph(1)->setName("湿度(%)");
    m_envChart->graph(2)->setName("空气质量(%)");

    m_envChart->xAxis->setLabel("数据序号");
    m_envChart->yAxis->setLabel("数值");
    m_envChart->legend->setVisible(true);
    m_envChart->axisRect()->setMinimumMargins(QMargins(20, 20, 20, 40));
}

void RecordWindow::onQueryOperationRecords()
{
    QString deviceType = m_deviceTypeCombo->currentText() == "全部" ? "" : m_deviceTypeCombo->currentText();
    QString operationType = m_operationTypeCombo->currentText() == "全部" ? "" : m_operationTypeCombo->currentText();
    QDateTime startTime = QDateTime(m_startDateEdit->date(), QTime(0, 0, 0));
    QDateTime endTime = QDateTime(m_endDateEdit->date(), QTime(23, 59, 59));

    QList<OperationRecord> records = queryOperationRecords(deviceType, operationType, startTime, endTime);

    m_opTable->setRowCount(0);
    for (const OperationRecord& record : records) {
        int row = m_opTable->rowCount();
        m_opTable->insertRow(row);
        m_opTable->setItem(row, 0, new QTableWidgetItem(record.username));
        m_opTable->setItem(row, 1, new QTableWidgetItem(record.deviceName));
        m_opTable->setItem(row, 2, new QTableWidgetItem(record.operation));
        m_opTable->setItem(row, 3, new QTableWidgetItem(record.result));
        m_opTable->setItem(row, 4, new QTableWidgetItem(record.operateTime.toString("yyyy-MM-dd HH:mm:ss")));
    }
}

void RecordWindow::onQueryEnvData()
{
    QDateTime startTime = QDateTime::currentDateTime().addDays(-7);
    QDateTime endTime = QDateTime::currentDateTime();
    QList<EnvData> envDatas = queryEnvData(startTime, endTime);

    m_envTable->setRowCount(0);
    QVector<double> xData, temps, hums, aqs;

    for (int i = 0; i < envDatas.size(); ++i) {
        const EnvData& data = envDatas[i];
        int row = m_envTable->rowCount();
        m_envTable->insertRow(row);
        m_envTable->setItem(row, 0, new QTableWidgetItem(data.recordTime.toString("yyyy-MM-dd HH:mm")));
        m_envTable->setItem(row, 1, new QTableWidgetItem(QString::number(data.temperature, 'f', 1)));
        m_envTable->setItem(row, 2, new QTableWidgetItem(QString::number(data.humidity, 'f', 1)));
        m_envTable->setItem(row, 3, new QTableWidgetItem(QString::number(data.airQuality)));

        xData.append(i);
        temps.append(data.temperature);
        hums.append(data.humidity);
        aqs.append(data.airQuality);
    }

    if (m_envChart) {
        m_envChart->graph(0)->setData(xData, temps);
        m_envChart->graph(1)->setData(xData, hums);
        m_envChart->graph(2)->setData(xData, aqs);

        m_envChart->rescaleAxes();
        if (!xData.isEmpty()) {
            m_envChart->xAxis->setRange(-1, xData.last() + 1);
        }
        m_envChart->replot();
    }
}

void RecordWindow::onExportOperationRecords()
{
    if (m_opTable->rowCount() == 0) {
        QMessageBox::warning(this, "提示", "暂无操作记录可导出！");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(this, "导出操作记录",
                                                    QString("操作记录_%1.csv").arg(QDate::currentDate().toString("yyyyMMdd")),
                                                    "Excel文件 (*.xlsx);;CSV文件 (*.csv)");
    if (!filePath.endsWith(".csv") && !filePath.endsWith(".xlsx")) {
        filePath += ".csv";
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, "错误", "文件打开失败：" + file.errorString());
        return;
    }

    file.write("\xEF\xBB\xBF");
    QTextStream out(&file);
    // 表头：操作时间占两格（E+F）
    out << "操作人,设备名称,操作内容,操作结果,,操作时间\n";

    for (int i = 0; i < m_opTable->rowCount(); ++i) {
        QStringList rowData;
        for (int j = 0; j < m_opTable->columnCount(); ++j) {
            QTableWidgetItem *item = m_opTable->item(i, j);
            QString text = item ? item->text() : "";
            text = text.replace("\"", "\"\"");

            // 时间列（索引4）：前面加逗号，让它占两格
            if (j == 4) {
                text = "," + text;
            }

            rowData.append("\"" + text + "\"");
        }
        out << rowData.join(",") << "\n";
    }

    file.close();
    QMessageBox::information(this, "成功", "操作记录导出成功！");
}

void RecordWindow::onExportEnvData()
{
    if (m_envTable->rowCount() == 0) {
        QMessageBox::warning(this, "提示", "暂无环境数据可导出！");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(this, "导出环境数据",
                                                    QString("环境数据_%1.csv").arg(QDate::currentDate().toString("yyyyMMdd")),
                                                    "Excel文件 (*.xlsx);;CSV文件 (*.csv)");
    if (!filePath.endsWith(".csv") && !filePath.endsWith(".xlsx")) {
        filePath += ".csv";
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, "错误", "文件打开失败：" + file.errorString());
        return;
    }

    file.write("\xEF\xBB\xBF");
    QTextStream out(&file);
    // 表头对应调整：给时间列预留一格（加逗号）
    out << "温度(℃),湿度(%),空气质量(%),记录时间\n";

    for (int i = 0; i < m_envTable->rowCount(); ++i) {
        QStringList rowData;
        // 先收集温度、湿度、空气质量
        for (int j = 1; j < m_envTable->columnCount(); ++j) {
            QTableWidgetItem *item = m_envTable->item(i, j);
            QString text = item ? item->text() : "";
            text = text.replace("\"", "\"\"");
            rowData.append("\"" + text + "\"");
        }
        // 添加记录时间，前面加逗号占两格
        QTableWidgetItem *timeItem = m_envTable->item(i, 0);
        QString timeText = timeItem ? timeItem->text() : "";
        timeText = timeText.replace("\"", "\"\"");
        timeText = ", " + timeText;

        rowData.append("\"" + timeText + "\"");

        // 写入一行数据
        out << rowData.join(",") << "\n";
    }

    file.close();
    QMessageBox::information(this, "成功", "环境数据导出成功！");
}
