#ifndef RECORDWINDOW_H
#define RECORDWINDOW_H
#include <QDialog>
#include <QComboBox>
#include <QDateEdit>
#include <QTableWidget>
#include "qcustomplot.h"
#include "db_extension.h"

class RecordWindow : public QDialog
{
    Q_OBJECT
public:
    explicit RecordWindow(const QString& username, QWidget *parent = nullptr);

private slots:
    void onQueryOperationRecords();    // 查询操作记录
    void onQueryEnvData();             // 查询环境数据
    void onExportOperationRecords();   // 导出操作记录
    void onExportEnvData();            // 导出环境数据

private:
    QComboBox *m_deviceTypeCombo;
    QComboBox *m_operationTypeCombo;
    QDateEdit *m_startDateEdit;
    QDateEdit *m_endDateEdit;
    QTableWidget *m_opTable;
    QTableWidget *m_envTable;
    QCustomPlot *m_envChart;
    QString m_username;

    void initUI();
    void initChart();
};

#endif // RECORDWINDOW_H
