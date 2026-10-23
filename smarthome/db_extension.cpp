#include "db_extension.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>
#include <QRandomGenerator>

// 数据库连接名称（避免与主数据库冲突）
#define DB_EXTENSION_NAME "smart_home_extension"

// 获取数据库连接（单例模式，避免重复连接）
QSqlDatabase getDbExtensionConnection()
{
    // 检查是否已有连接
    if (QSqlDatabase::contains(DB_EXTENSION_NAME)) {
        return QSqlDatabase::database(DB_EXTENSION_NAME);
    }

    // 创建新连接
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", DB_EXTENSION_NAME);
    // 设置数据库文件路径（保存在程序运行目录）
    db.setDatabaseName("smart_home_extension.db");

    return db;
}

// 初始化扩展数据库表（操作记录+环境数据+家具表）
bool initExtensionDb()
{
    QSqlDatabase db = getDbExtensionConnection();
    if (!db.open()) {
        qDebug() << "[扩展数据库] 连接失败：" << db.lastError().text();
        return false;
    }

    // ========== 1. 创建操作记录表 ==========
    QSqlQuery opQuery(db);
    QString opTableSql = R"(
        CREATE TABLE IF NOT EXISTS operation_records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL,
            device_name TEXT NOT NULL,
            operation TEXT NOT NULL,
            result TEXT NOT NULL,
            operate_time TEXT NOT NULL
        )
    )";
    if (!opQuery.exec(opTableSql)) {
        qDebug() << "[扩展数据库] 创建操作记录表失败：" << opQuery.lastError().text();
        db.close();
        return false;
    }

    // ========== 2. 创建环境数据表 ==========
    QSqlQuery envQuery(db);
    QString envTableSql = R"(
        CREATE TABLE IF NOT EXISTS env_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            record_time TEXT NOT NULL,
            temperature REAL NOT NULL,
            humidity REAL NOT NULL,
            airQuality INTEGER NOT NULL
        )
    )";
    if (!envQuery.exec(envTableSql)) {
        qDebug() << "[扩展数据库] 创建环境数据表失败：" << envQuery.lastError().text();
        db.close();
        return false;
    }

    // ========== 3. 创建家具表 ==========
    QSqlQuery furnitureQuery(db);
    QString furnitureTableSql = R"(
        CREATE TABLE IF NOT EXISTS furniture (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            type TEXT NOT NULL,
            name TEXT NOT NULL,
            room TEXT NOT NULL,
            status INTEGER DEFAULT 0,
            brightness INTEGER DEFAULT 80,
            temperature INTEGER DEFAULT 26,
            angle INTEGER DEFAULT 0,
            openness INTEGER DEFAULT 50,
            fault INTEGER DEFAULT 0
        )
    )";
    if (!furnitureQuery.exec(furnitureTableSql)) {
        qDebug() << "[扩展数据库] 创建家具表失败：" << furnitureQuery.lastError().text();
        db.close();
        return false;
    }

    // ========== 4. 初始化默认家具数据（如果表为空） ==========
    QSqlQuery checkQuery(db);
    checkQuery.exec("SELECT COUNT(*) FROM furniture");
    if (checkQuery.next() && checkQuery.value(0).toInt() == 0) {
        QSqlQuery insertQuery(db);

        // 灯光：客厅2，厨房1，卫生间1，卧室1
        insertQuery.prepare("INSERT INTO furniture (type, name, room, status, brightness, fault) VALUES (?, ?, ?, ?, ?, ?)");

        // 客厅灯1
        insertQuery.addBindValue("light");
        insertQuery.addBindValue("客厅灯1");
        insertQuery.addBindValue("客厅");
        insertQuery.addBindValue(0);
        insertQuery.addBindValue(80);
        insertQuery.addBindValue(0);
        insertQuery.exec();

        // 客厅灯2
        insertQuery.addBindValue("light");
        insertQuery.addBindValue("客厅灯2");
        insertQuery.addBindValue("客厅");
        insertQuery.addBindValue(0);
        insertQuery.addBindValue(80);
        insertQuery.addBindValue(0);
        insertQuery.exec();

        // 厨房灯
        insertQuery.addBindValue("light");
        insertQuery.addBindValue("厨房灯");
        insertQuery.addBindValue("厨房");
        insertQuery.addBindValue(0);
        insertQuery.addBindValue(80);
        insertQuery.addBindValue(0);
        insertQuery.exec();

        // 卫生间灯
        insertQuery.addBindValue("light");
        insertQuery.addBindValue("卫生间灯");
        insertQuery.addBindValue("卫生间");
        insertQuery.addBindValue(0);
        insertQuery.addBindValue(80);
        insertQuery.addBindValue(0);
        insertQuery.exec();

        // 卧室灯
        insertQuery.addBindValue("light");
        insertQuery.addBindValue("卧室灯");
        insertQuery.addBindValue("卧室");
        insertQuery.addBindValue(0);
        insertQuery.addBindValue(80);
        insertQuery.addBindValue(0);
        insertQuery.exec();

        // 空调：客厅1
        insertQuery.addBindValue("aircon");
        insertQuery.addBindValue("客厅空调");
        insertQuery.addBindValue("客厅");
        insertQuery.addBindValue(0);
        insertQuery.addBindValue(26);
        insertQuery.addBindValue(0);
        insertQuery.exec();

        // 摄像头：客厅1，厨房1，卧室1
        insertQuery.addBindValue("camera");
        insertQuery.addBindValue("客厅摄像头");
        insertQuery.addBindValue("客厅");
        insertQuery.addBindValue(0);
        insertQuery.addBindValue(0);
        insertQuery.addBindValue(0);
        insertQuery.exec();

        insertQuery.addBindValue("camera");
        insertQuery.addBindValue("厨房摄像头");
        insertQuery.addBindValue("厨房");
        insertQuery.addBindValue(0);
        insertQuery.addBindValue(0);
        insertQuery.addBindValue(0);
        insertQuery.exec();

        insertQuery.addBindValue("camera");
        insertQuery.addBindValue("卧室摄像头");
        insertQuery.addBindValue("卧室");
        insertQuery.addBindValue(0);
        insertQuery.addBindValue(0);
        insertQuery.addBindValue(0);
        insertQuery.exec();

        // 窗帘：客厅1，卧室1
        insertQuery.addBindValue("curtain");
        insertQuery.addBindValue("客厅窗帘");
        insertQuery.addBindValue("客厅");
        insertQuery.addBindValue(0);
        insertQuery.addBindValue(50);
        insertQuery.addBindValue(0);
        insertQuery.exec();

        insertQuery.addBindValue("curtain");
        insertQuery.addBindValue("卧室窗帘");
        insertQuery.addBindValue("卧室");
        insertQuery.addBindValue(0);
        insertQuery.addBindValue(50);
        insertQuery.addBindValue(0);
        insertQuery.exec();
    }

    qDebug() << "[扩展数据库] 表初始化成功";
    db.close();
    return true;
}

// 插入操作记录
bool insertOperationRecord(const OperationRecord& record)
{
    QSqlDatabase db = getDbExtensionConnection();
    if (!db.open()) {
        qDebug() << "[操作记录] 数据库连接失败：" << db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO operation_records
        (username, device_name, operation, result, operate_time)
        VALUES (?, ?, ?, ?, ?)
    )");
    query.addBindValue(record.username);
    query.addBindValue(record.deviceName);
    query.addBindValue(record.operation);
    query.addBindValue(record.result);
    query.addBindValue(record.operateTime.toString("yyyy-MM-dd HH:mm:ss"));

    bool success = query.exec();
    if (!success) {
        qDebug() << "[操作记录] 插入失败：" << query.lastError().text()
            << "参数：" << record.username << record.deviceName;
    } else {
        qDebug() << "[操作记录] 插入成功：" << record.deviceName << record.operation;
    }

    db.close();
    return success;
}

// 查询操作记录（支持多条件筛选）
QList<OperationRecord> queryOperationRecords(const QString& deviceType,
                                             const QString& operationType,
                                             const QDateTime& startTime,
                                             const QDateTime& endTime)
{
    QList<OperationRecord> records;
    QSqlDatabase db = getDbExtensionConnection();

    if (!db.open()) {
        qDebug() << "[操作记录] 查询 - 数据库连接失败：" << db.lastError().text();
        return records;
    }

    QString sql = "SELECT username, device_name, operation, result, operate_time FROM operation_records WHERE 1=1";
    QList<QVariant> bindValues;

    if (!deviceType.isEmpty()) {
        sql += " AND device_name LIKE ?";
        bindValues.append(QString("%%1%").arg(deviceType));
    }

    if (!operationType.isEmpty()) {
        sql += " AND operation LIKE ?";
        bindValues.append(QString("%%1%").arg(operationType));
    }

    if (startTime.isValid()) {
        sql += " AND operate_time >= ?";
        bindValues.append(startTime.toString("yyyy-MM-dd HH:mm:ss"));
    }

    if (endTime.isValid()) {
        sql += " AND operate_time <= ?";
        bindValues.append(endTime.toString("yyyy-MM-dd HH:mm:ss"));
    }

    sql += " ORDER BY operate_time DESC";

    QSqlQuery query(db);
    query.prepare(sql);

    for (int i = 0; i < bindValues.size(); ++i) {
        query.addBindValue(bindValues[i]);
    }

    if (!query.exec()) {
        qDebug() << "[操作记录] 查询SQL执行失败：" << query.lastError().text()
            << "SQL：" << sql;
        db.close();
        return records;
    }

    while (query.next()) {
        OperationRecord record;
        record.username = query.value(0).toString();
        record.deviceName = query.value(1).toString();
        record.operation = query.value(2).toString();
        record.result = query.value(3).toString();
        record.operateTime = QDateTime::fromString(query.value(4).toString(), "yyyy-MM-dd HH:mm:ss");

        records.append(record);
    }

    qDebug() << "[操作记录] 查询完成，共" << records.size() << "条记录";
    db.close();
    return records;
}

// 插入环境数据
bool insertEnvData(const EnvData& data)
{
    QSqlDatabase db = getDbExtensionConnection();
    if (!db.open()) {
        qDebug() << "[环境数据] 数据库连接失败：" << db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO env_data
        (record_time, temperature, humidity, airQuality)
        VALUES (?, ?, ?, ?)
    )");
    query.addBindValue(data.recordTime.toString("yyyy-MM-dd HH:mm:ss"));
    query.addBindValue(data.temperature);
    query.addBindValue(data.humidity);
    query.addBindValue(data.airQuality);

    bool success = query.exec();
    if (!success) {
        qDebug() << "[环境数据] 插入失败：" << query.lastError().text();
    } else {
        QDateTime delTime = QDateTime::currentDateTime().addDays(-7);
        QSqlQuery delQuery(db);
        delQuery.prepare("DELETE FROM env_data WHERE record_time < ?");
        delQuery.addBindValue(delTime.toString("yyyy-MM-dd HH:mm:ss"));
        delQuery.exec();

        qDebug() << "[环境数据] 插入成功，温度：" << data.temperature << "℃";
    }

    db.close();
    return success;
}

// 查询环境数据（按时间范围）
QList<EnvData> queryEnvData(const QDateTime& startTime, const QDateTime& endTime)
{
    QList<EnvData> envDatas;
    QSqlDatabase db = getDbExtensionConnection();

    if (!db.open()) {
        qDebug() << "[环境数据] 查询 - 数据库连接失败：" << db.lastError().text();
        return envDatas;
    }

    QString sql = "SELECT record_time, temperature, humidity, airQuality FROM env_data WHERE 1=1";
    QList<QVariant> bindValues;

    if (startTime.isValid()) {
        sql += " AND record_time >= ?";
        bindValues.append(startTime.toString("yyyy-MM-dd HH:mm:ss"));
    }

    if (endTime.isValid()) {
        sql += " AND record_time <= ?";
        bindValues.append(endTime.toString("yyyy-MM-dd HH:mm:ss"));
    }

    sql += " ORDER BY record_time ASC";

    QSqlQuery query(db);
    query.prepare(sql);

    for (int i = 0; i < bindValues.size(); ++i) {
        query.addBindValue(bindValues[i]);
    }

    if (!query.exec()) {
        qDebug() << "[环境数据] 查询SQL执行失败：" << query.lastError().text()
            << "SQL：" << sql;
        db.close();
        return envDatas;
    }

    while (query.next()) {
        EnvData data;
        data.recordTime = QDateTime::fromString(query.value(0).toString(), "yyyy-MM-dd HH:mm:ss");
        data.temperature = query.value(1).toDouble();
        data.humidity = query.value(2).toDouble();
        data.airQuality = query.value(3).toInt();

        envDatas.append(data);
    }

    qDebug() << "[环境数据] 查询完成，共" << envDatas.size() << "条记录";
    db.close();
    return envDatas;
}

// 生成模拟环境数据（用于测试）
EnvData generateMockEnvData()
{
    EnvData data;
    data.recordTime = QDateTime::currentDateTime();
    data.temperature = 20 + QRandomGenerator::global()->bounded(10.0);
    data.humidity = 40 + QRandomGenerator::global()->bounded(30.0);
    data.airQuality = 60 + QRandomGenerator::global()->bounded(40);

    return data;
}
