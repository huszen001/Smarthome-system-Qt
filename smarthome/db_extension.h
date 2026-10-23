#ifndef DB_EXTENSION_H
#define DB_EXTENSION_H

#include <QObject>
#include <QSqlDatabase>
#include <QDateTime>
#include <QList>

// 操作记录结构体
struct OperationRecord
{
    QString username;      // 操作人
    QString deviceName;    // 设备名称
    QString operation;     // 操作内容
    QString result;        // 操作结果（成功/失败）
    QDateTime operateTime; // 操作时间
};

// 环境数据结构体
struct EnvData
{
    QDateTime recordTime;  // 记录时间
    double temperature;    // 温度（℃）
    double humidity;       // 湿度（%）
    int airQuality;        // 空气质量（%）
};

// 家具结构体
struct Furniture
{
    int id;
    QString type;      // light/aircon/camera/curtain
    QString name;      // 显示名称
    QString room;      // 客厅/厨房/卫生间/卧室
    int status;        // 0关 1开
    int brightness;    // 灯光亮度
    int temperature;   // 空调温度
    int angle;         // 摄像头角度
    int openness;      // 窗帘开度
    int fault;         // 0正常 1故障
};

// 获取扩展数据库连接
QSqlDatabase getDbExtensionConnection();

// 初始化扩展数据库表
bool initExtensionDb();

// 操作记录相关
bool insertOperationRecord(const OperationRecord& record);
QList<OperationRecord> queryOperationRecords(const QString& deviceType = "",
                                             const QString& operationType = "",
                                             const QDateTime& startTime = QDateTime(),
                                             const QDateTime& endTime = QDateTime());

// 环境数据相关
bool insertEnvData(const EnvData& data);
QList<EnvData> queryEnvData(const QDateTime& startTime = QDateTime(),
                            const QDateTime& endTime = QDateTime());

// 生成模拟环境数据（测试用）
EnvData generateMockEnvData();

#endif // DB_EXTENSION_H
