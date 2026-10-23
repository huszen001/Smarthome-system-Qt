#include "mainwindow.h"
#include "devicecontroller.h"
#include "db_extension.h"
#include "record_window.h"
#include <cstdlib>
#include <QMenu>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QList>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QGroupBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QEvent>
#include <QMouseEvent>
#include <QDateTime>
#include <QSet>
#include <QSettings>
#include <QFileDialog>
#include <QComboBox>
#include <QTableWidget>
#include <QFile>
#include <QSoundEffect>
#include <QUrl>
#include <QDebug>



#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

// ==================== JSON序列化辅助函数 ====================
static QString serializeDeviceStates(const QMap<int, bool> &deviceStatus,
                                     const QMap<int, int> &deviceValues)
{
    QJsonArray statesArray;

    // 获取所有设备ID
    QSet<int> allIds;
    for (auto it = deviceStatus.begin(); it != deviceStatus.end(); ++it) {
        allIds.insert(it.key());
    }
    for (auto it = deviceValues.begin(); it != deviceValues.end(); ++it) {
        allIds.insert(it.key());
    }

    // 转换为JSON数组
    QList<int> sortedIds = allIds.values();
    std::sort(sortedIds.begin(), sortedIds.end());

    for (int id : sortedIds) {
        QJsonObject deviceObj;
        deviceObj["id"] = id;
        deviceObj["status"] = deviceStatus.contains(id) ? deviceStatus[id] : false;
        deviceObj["value"] = deviceValues.contains(id) ? deviceValues[id] : 0;
        statesArray.append(deviceObj);
    }

    QJsonDocument doc(statesArray);
    return QString(doc.toJson(QJsonDocument::Compact));
}

static void deserializeDeviceStates(const QString &jsonStr,
                                    QMap<int, bool> &deviceStatus,
                                    QMap<int, int> &deviceValues)
{
    deviceStatus.clear();
    deviceValues.clear();

    if (jsonStr.isEmpty()) return;

    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isArray()) return;

    QJsonArray statesArray = doc.array();
    for (const QJsonValue &value : statesArray) {
        QJsonObject obj = value.toObject();
        int id = obj["id"].toInt();
        deviceStatus[id] = obj["status"].toBool();
        deviceValues[id] = obj["value"].toInt();
    }
}


// ==================== 数据库连接函数 ====================
static QSqlDatabase getDbConnection()
{
    static QSqlDatabase db;

    if (!db.isOpen() || !db.isValid()) {
        if (QSqlDatabase::contains("qt_sql_default_connection")) {
            db = QSqlDatabase::database("qt_sql_default_connection");
        } else {
            db = QSqlDatabase::addDatabase("QSQLITE");
            db.setDatabaseName("smarthome.db");
        }

        if (!db.isOpen()) {
            db.open();
        }
    }
    return db;
}

// ==================== 密码加密函数 ====================
static QString encryptPassword(QString pwd)
{
    return QString(QCryptographicHash::hash(pwd.toUtf8(), QCryptographicHash::Md5).toHex());
}

// ==================== 初始化主数据库 ====================
static bool initDb()
{
    QSqlDatabase db = getDbConnection();
    if (!db.open()) return false;

    QSqlQuery query;
    query.exec("CREATE TABLE IF NOT EXISTS users (username TEXT PRIMARY KEY, password TEXT)");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS devices (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            type TEXT NOT NULL,
            group_name TEXT NOT NULL,
            status INTEGER DEFAULT 1
        )
    )");

    query.exec("CREATE TABLE IF NOT EXISTS custom_scenes ("
               "name TEXT PRIMARY KEY, "
               "lightOn INTEGER, lightBright INTEGER, "
               "airconOn INTEGER, airconTemp INTEGER, "
               "curtainOn INTEGER, curtainOpenness INTEGER, "
               "cameraOn INTEGER, cameraAngle INTEGER)");

    query.prepare("INSERT OR IGNORE INTO users VALUES(?, ?)");
    query.addBindValue("admin");
    query.addBindValue(encryptPassword("123456"));
    query.exec();

    // ========== 添加内置设备到 devices 表 ==========
    // 检查是否已有设备数据
    QSqlQuery checkQuery("SELECT COUNT(*) FROM devices", db);
    checkQuery.next();
    int deviceCount = checkQuery.value(0).toInt();

    // 如果 devices 表为空，则插入内置设备
    if (deviceCount == 0) {
        qDebug() << "初始化内置设备到 devices 表...";

        // 灯光设备
        QStringList lightDevices = {"客厅灯1", "客厅灯2", "厨房灯", "卫生间灯", "卧室灯"};
        for (const QString& name : lightDevices) {
            QSqlQuery insertQuery(db);
            insertQuery.prepare("INSERT INTO devices (name, type, group_name, status) VALUES (?, ?, ?, ?)");

            QString room;
            if (name.contains("客厅")) room = "客厅";
            else if (name.contains("厨房")) room = "厨房";
            else if (name.contains("卫生间")) room = "卫生间";
            else if (name.contains("卧室")) room = "卧室";

            insertQuery.addBindValue(name);
            insertQuery.addBindValue("light");
            insertQuery.addBindValue(room);
            insertQuery.addBindValue(0); // 默认离线
            insertQuery.exec();
        }

        // 空调设备
        QStringList airconDevices = {"客厅空调"};
        for (const QString& name : airconDevices) {
            QSqlQuery insertQuery(db);
            insertQuery.prepare("INSERT INTO devices (name, type, group_name, status) VALUES (?, ?, ?, ?)");

            QString room;
            if (name.contains("客厅")) room = "客厅";

            insertQuery.addBindValue(name);
            insertQuery.addBindValue("aircon");
            insertQuery.addBindValue(room);
            insertQuery.addBindValue(0);
            insertQuery.exec();
        }

        // 摄像头设备
        QStringList cameraDevices = {"客厅摄像头", "厨房摄像头", "卧室摄像头"};
        for (const QString& name : cameraDevices) {
            QSqlQuery insertQuery(db);
            insertQuery.prepare("INSERT INTO devices (name, type, group_name, status) VALUES (?, ?, ?, ?)");

            QString room;
            if (name.contains("客厅")) room = "客厅";
            else if (name.contains("厨房")) room = "厨房";
            else if (name.contains("卧室")) room = "卧室";

            insertQuery.addBindValue(name);
            insertQuery.addBindValue("camera");
            insertQuery.addBindValue(room);
            insertQuery.addBindValue(0);
            insertQuery.exec();
        }

        // 窗帘设备
        QStringList curtainDevices = {"客厅窗帘", "卧室窗帘"};
        for (const QString& name : curtainDevices) {
            QSqlQuery insertQuery(db);
            insertQuery.prepare("INSERT INTO devices (name, type, group_name, status) VALUES (?, ?, ?, ?)");

            QString room;
            if (name.contains("客厅")) room = "客厅";
            else if (name.contains("卧室")) room = "卧室";

            insertQuery.addBindValue(name);
            insertQuery.addBindValue("curtain");
            insertQuery.addBindValue(room);
            insertQuery.addBindValue(0);
            insertQuery.exec();
        }

        qDebug() << "内置设备初始化完成";
    }

    db.close();
    return true;
}
static bool checkLogin(QString username, QString password)
{
    QSqlDatabase db = getDbConnection();
    if (!db.open()) return false;

    QSqlQuery query;
    query.prepare("SELECT * FROM users WHERE username=? AND password=?");
    query.addBindValue(username);
    query.addBindValue(encryptPassword(password));
    query.exec();

    bool found = query.next();
    db.close();
    return found;
}

static bool addUser(QString username, QString password)
{
    QSqlDatabase db = getDbConnection();
    if (!db.open()) return false;

    QSqlQuery query;
    query.prepare("INSERT INTO users VALUES(?, ?)");
    query.addBindValue(username);
    query.addBindValue(encryptPassword(password));

    bool success = query.exec();
    db.close();
    return success;
}

static bool updatePassword(QString username, QString newPassword)
{
    QSqlDatabase db = getDbConnection();
    if (!db.open()) return false;

    QSqlQuery query;
    query.prepare("UPDATE users SET password=? WHERE username=?");
    query.addBindValue(encryptPassword(newPassword));
    query.addBindValue(username);

    bool success = query.exec() && (query.numRowsAffected() > 0);
    db.close();
    return success;
}
// ==================== 场景模式相关函数实现 ====================

void MainWindow::loadSceneModes()
{
    sceneModes.clear();

    QSqlDatabase db = getDbConnection();
    if (!db.open()) {
        qDebug() << "加载场景失败：数据库连接失败";
        return;
    }

    QSqlQuery query;
    query.exec("SELECT name, device_states FROM custom_scenes ORDER BY name LIMIT 2");
    while (query.next()) {
        SceneMode scene;
        scene.name = query.value(0).toString();

        // 从JSON反序列化设备状态
        QString deviceStatesJson = query.value(1).toString();
        deserializeDeviceStates(deviceStatesJson, scene.deviceStatus, scene.deviceValues);

        // 为了兼容旧代码，同时设置客厅设备的值
        Furniture* livingRoomLight = getLivingRoomLight();
        Furniture* livingRoomAircon = getLivingRoomAircon();

        if (livingRoomLight) {
            int id = livingRoomLight->id;
            scene.lightOn = scene.deviceStatus.contains(id) ? scene.deviceStatus[id] : false;
            scene.lightBright = scene.deviceValues.contains(id) ? scene.deviceValues[id] : 50;
        }

        if (livingRoomAircon) {
            int id = livingRoomAircon->id;
            scene.airconOn = scene.deviceStatus.contains(id) ? scene.deviceStatus[id] : false;
            scene.airconTemp = scene.deviceValues.contains(id) ? scene.deviceValues[id] : 24;
        }

        // 查找客厅窗帘和摄像头
        for (const Furniture &f : m_furniture) {
            if (f.type == "curtain" && f.room == "客厅") {
                scene.curtainOn = scene.deviceStatus.contains(f.id) ? scene.deviceStatus[f.id] : false;
                scene.curtainOpenness = scene.deviceValues.contains(f.id) ? scene.deviceValues[f.id] : 50;
            }
            if (f.type == "camera" && f.room == "客厅") {
                scene.cameraOn = scene.deviceStatus.contains(f.id) ? scene.deviceStatus[f.id] : false;
                scene.cameraAngle = scene.deviceValues.contains(f.id) ? scene.deviceValues[f.id] : 0;
            }
        }

        sceneModes.append(scene);
    }

    db.close();
}

void MainWindow::saveSceneModes()
{
    while (sceneModes.size() > 2) sceneModes.removeLast();

    QSqlDatabase db = getDbConnection();
    if (!db.open()) {
        qDebug() << "保存场景失败：数据库连接失败";
        return;
    }

    QSqlQuery query;
    query.exec("DELETE FROM custom_scenes");

    for (const SceneMode &scene : sceneModes) {
        // 序列化所有设备状态为JSON
        QString deviceStatesJson = serializeDeviceStates(scene.deviceStatus, scene.deviceValues);

        query.prepare("INSERT INTO custom_scenes (name, device_states) VALUES(?, ?)");
        query.addBindValue(scene.name);
        query.addBindValue(deviceStatesJson);

        if (!query.exec()) {
            qDebug() << "保存场景失败：" << query.lastError().text();
        }
    }

    db.close();
    updateCustomButtons();
}

void MainWindow::updateCustomButtons()
{
    if (sceneModes.size() > 0) {
        custom1Btn->setText(sceneModes[0].name);
        custom1Btn->setStyleSheet("QPushButton { background-color: #f39c12; color: white; border-radius: 5px; }");
    } else {
        custom1Btn->setText("未定义一");
        custom1Btn->setStyleSheet("QPushButton { background-color: #95a5a6; color: white; border-radius: 5px; }");
    }

    if (sceneModes.size() > 1) {
        custom2Btn->setText(sceneModes[1].name);
        custom2Btn->setStyleSheet("QPushButton { background-color: #f39c12; color: white; border-radius: 5px; }");
    } else {
        custom2Btn->setText("未定义二");
        custom2Btn->setStyleSheet("QPushButton { background-color: #95a5a6; color: white; border-radius: 5px; }");
    }
}

void MainWindow::applySceneMode(const SceneMode &scene)
{
    // 检查故障（所有设备）
    for (const Furniture &f : m_furniture) {
        if (f.fault && scene.deviceStatus.contains(f.id) && scene.deviceStatus[f.id]) {
            QMessageBox::warning(this, "无法执行模式",
                                 QString("%1处于故障状态，请先修复后再执行场景。").arg(f.name));
            return;
        }
    }

    // 应用场景到所有设备
    for (Furniture &f : m_furniture) {
        if (scene.deviceStatus.contains(f.id)) {
            // 设备在场景中有定义
            f.status = scene.deviceStatus[f.id] ? 1 : 0;

            if (scene.deviceValues.contains(f.id)) {
                int value = scene.deviceValues[f.id];

                if (f.type == "light") {
                    f.brightness = value;
                } else if (f.type == "aircon") {
                    f.temperature = value;
                } else if (f.type == "curtain") {
                    f.openness = value;
                } else if (f.type == "camera") {
                    f.angle = value;
                }
            }
        } else {
            // 设备在场景中没有定义，设置为关闭
            f.status = 0;
        }

        updateFurnitureInDb(f);
    }

    // 重新加载家具数据
    loadFurnitureFromDb();

    // 更新首页客厅设备状态
    Furniture* livingRoomLight = getLivingRoomLight();
    Furniture* livingRoomAircon = getLivingRoomAircon();

    if (livingRoomLight) {
        if (livingRoomLight->status) {
            livingRoomLightStatusLabel->setText("客厅灯: 开启");
            livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #f39c12;");
            livingRoomLightBrightnessLabel->setText(QString("亮度: %1%").arg(livingRoomLight->brightness));
        } else {
            livingRoomLightStatusLabel->setText("客厅灯: 关闭");
            livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");
            livingRoomLightBrightnessLabel->setText("亮度: --");
        }
    }

    if (livingRoomAircon) {
        if (livingRoomAircon->status) {
            livingRoomAirconStatusLabel->setText("客厅空调: 开启");
            livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #3498db;");
            livingRoomAirconTempLabel->setText(QString("温度: %1°C").arg(livingRoomAircon->temperature));
        } else {
            livingRoomAirconStatusLabel->setText("客厅空调: 关闭");
            livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");
            livingRoomAirconTempLabel->setText("温度: --");
        }
    }

    // 构建状态消息
    QString statusMsg = "自定义模式 \"" + scene.name + "\" 已启动\n\n";

    int enabledCount = 0;
    for (const Furniture &f : m_furniture) {
        if (scene.deviceStatus.contains(f.id) && scene.deviceStatus[f.id]) {
            enabledCount++;
            statusMsg += "✓ " + f.room + " " + f.name;

            if (f.type == "light") {
                statusMsg += " (亮度:" + QString::number(f.brightness) + "%)";
            } else if (f.type == "aircon") {
                statusMsg += " (温度:" + QString::number(f.temperature) + "°C)";
            } else if (f.type == "curtain") {
                statusMsg += " (开度:" + QString::number(f.openness) + "%)";
            } else if (f.type == "camera") {
                statusMsg += " (角度:" + QString::number(f.angle) + "°)";
            }
            statusMsg += "\n";
        }
    }

    int totalDevices = m_furniture.size();
    int autoClosed = totalDevices - enabledCount;
    if (autoClosed > 0) {
        statusMsg += QString("\n其他 %1 个设备已自动关闭").arg(autoClosed);
    }

    QTimer::singleShot(500, this, [=]() {
        QMessageBox::information(this, scene.name, statusMsg);
    });
}
// ==================== 登录相关函数实现 ====================
void MainWindow::doLogin()
{
    QString user = loginUsername->text().trimmed();
    QString pwd = loginPassword->text();

    if (user.isEmpty() || pwd.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入用户名和密码");
        return;
    }

    if (checkLogin(user, pwd)) {
        welcomeLabel->setText("欢迎 " + user);
        m_currentUser = user;
        alarmEnabled = true;
        deviceCheckCounter = 0;

        // 重新加载家具数据
        loadFurnitureFromDb();

        // 更新首页客厅设备状态和按钮样式
        Furniture* livingRoomLight = getLivingRoomLight();
        Furniture* livingRoomAircon = getLivingRoomAircon();

        if (livingRoomLight) {
            if (livingRoomLight->fault) {
                livingRoomLightStatusLabel->setText("客厅灯: 故障");
                livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #e74c3c; font-weight: bold;");
                livingRoomLightBrightnessLabel->setText("亮度: --");
                livingRoomLightBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-size: 14px; border-radius: 5px; }"
                                                  "QPushButton:hover { background-color: #c0392b; }");
            } else if (livingRoomLight->status) {
                livingRoomLightStatusLabel->setText("客厅灯: 开启");
                livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #f39c12;");
                livingRoomLightBrightnessLabel->setText(QString("亮度: %1%").arg(livingRoomLight->brightness));
                livingRoomLightBtn->setStyleSheet("QPushButton { background-color: #f1c40f; font-size: 14px; border-radius: 5px; }"
                                                  "QPushButton:hover { background-color: #f39c12; }");
            } else {
                livingRoomLightStatusLabel->setText("客厅灯: 关闭");
                livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");
                livingRoomLightBrightnessLabel->setText("亮度: --");
                livingRoomLightBtn->setStyleSheet("QPushButton { background-color: #f1c40f; font-size: 14px; border-radius: 5px; }"
                                                  "QPushButton:hover { background-color: #f39c12; }");
            }
        }

        if (livingRoomAircon) {
            if (livingRoomAircon->fault) {
                livingRoomAirconStatusLabel->setText("客厅空调: 故障");
                livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #e74c3c; font-weight: bold;");
                livingRoomAirconTempLabel->setText("温度: --");
                livingRoomAirconBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-size: 14px; border-radius: 5px; }"
                                                   "QPushButton:hover { background-color: #c0392b; }");
            } else if (livingRoomAircon->status) {
                livingRoomAirconStatusLabel->setText("客厅空调: 开启");
                livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #3498db;");
                livingRoomAirconTempLabel->setText(QString("温度: %1°C").arg(livingRoomAircon->temperature));
                livingRoomAirconBtn->setStyleSheet("QPushButton { background-color: #3498db; color: white; font-size: 14px; border-radius: 5px; }"
                                                   "QPushButton:hover { background-color: #2980b9; }");
            } else {
                livingRoomAirconStatusLabel->setText("客厅空调: 关闭");
                livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");
                livingRoomAirconTempLabel->setText("温度: --");
                livingRoomAirconBtn->setStyleSheet("QPushButton { background-color: #3498db; color: white; font-size: 14px; border-radius: 5px; }"
                                                   "QPushButton:hover { background-color: #2980b9; }");
            }
        }

        stackedWidget->setCurrentWidget(homePage);
        loginPassword->clear();
        updateDeviceStats();
    } else {
        QMessageBox::warning(this, "提示", "用户名或密码错误");
        loginPassword->clear();
    }
}

void MainWindow::showRegister()
{
    regUsername->clear();
    regPassword->clear();
    regConfirmPassword->clear();
    stackedWidget->setCurrentWidget(registerPage);
}

void MainWindow::showReset()
{
    resetUsername->clear();
    resetNewPassword->clear();
    resetConfirmPassword->clear();
    stackedWidget->setCurrentWidget(resetPage);
}

void MainWindow::backToLogin()
{
    stackedWidget->setCurrentWidget(loginPage);
}

void MainWindow::doRegister()
{
    QString user = regUsername->text().trimmed();
    QString pwd = regPassword->text();
    QString confirm = regConfirmPassword->text();

    if (user.isEmpty() || pwd.isEmpty()) {
        QMessageBox::warning(this, "提示", "用户名和密码不能为空");
        return;
    }

    if (pwd != confirm) {
        QMessageBox::warning(this, "提示", "两次密码不一致");
        return;
    }

    if (addUser(user, pwd)) {
        QMessageBox::information(this, "成功", "注册成功，请登录");
        backToLogin();
        loginUsername->setText(user);
    } else {
        QMessageBox::warning(this, "提示", "用户名已存在");
    }
}

void MainWindow::doReset()
{
    QString user = resetUsername->text().trimmed();
    QString pwd = resetNewPassword->text();
    QString confirm = resetConfirmPassword->text();

    if (user.isEmpty() || pwd.isEmpty()) {
        QMessageBox::warning(this, "提示", "请填写完整信息");
        return;
    }

    if (pwd != confirm) {
        QMessageBox::warning(this, "提示", "两次密码不一致");
        return;
    }

    if (updatePassword(user, pwd)) {
        QMessageBox::information(this, "成功", "密码重置成功，请登录");
        backToLogin();
        loginUsername->setText(user);
    } else {
        QMessageBox::warning(this, "提示", "用户不存在");
    }
}

void MainWindow::doLogout()
{
    alarmEnabled = false;

    lightFault = false;
    airconFault = false;

    if (lightFixTimer && lightFixTimer->isActive()) lightFixTimer->stop();
    if (airconFixTimer && airconFixTimer->isActive()) airconFixTimer->stop();

    stackedWidget->setCurrentWidget(loginPage);
    loginUsername->clear();
    loginPassword->clear();
}
// ==================== 自定义场景管理 ====================

void MainWindow::addNewScene()
{
    if (sceneModes.size() >= 2) {
        QMessageBox::warning(this, "提示", "最多只能添加2个自定义模式！");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("新增自定义模式 (还可添加 " + QString::number(2 - sceneModes.size()) + " 个)");
    dialog.setFixedSize(450, 650);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);

    // 场景名称
    QHBoxLayout *nameLayout = new QHBoxLayout;
    nameLayout->addWidget(new QLabel("场景名称:"));
    QLineEdit *nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText("输入场景名称");
    nameLayout->addWidget(nameEdit);
    mainLayout->addLayout(nameLayout);

    // 创建可滚动的设备选择区域
    QLabel *deviceLabel = new QLabel("请选择要控制的设备：");
    deviceLabel->setStyleSheet("font-weight: bold; color: #2c3e50; margin-top: 10px;");
    mainLayout->addWidget(deviceLabel);

    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFixedHeight(450);
    scrollArea->setStyleSheet("QScrollArea { border: 1px solid #bdc3c7; border-radius: 5px; background-color: #f8f9f9; }");

    QWidget *scrollWidget = new QWidget;
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setSpacing(15);

    // 按房间分组显示设备
    QMap<QString, QList<Furniture>> devicesByRoom;
    for (const Furniture &f : m_furniture) {
        devicesByRoom[f.room].append(f);
    }

    QMap<int, QPair<QCheckBox*, QSlider*>> deviceControls;  // 存储每个设备的控件

    // 遍历每个房间
    QStringList roomOrder = {"客厅", "卧室", "厨房", "卫生间", "其他"};
    for (const QString &room : roomOrder) {
        if (!devicesByRoom.contains(room)) continue;

        QGroupBox *roomGroup = new QGroupBox(room);
        roomGroup->setStyleSheet("QGroupBox { font-size: 16px; font-weight: bold; margin-top: 10px; }");
        QVBoxLayout *roomLayout = new QVBoxLayout(roomGroup);
        roomLayout->setSpacing(10);

        QList<Furniture> devices = devicesByRoom[room];
        for (const Furniture &f : devices) {
            QWidget *deviceWidget = new QWidget;
            QHBoxLayout *deviceLayout = new QHBoxLayout(deviceWidget);
            deviceLayout->setContentsMargins(5, 5, 5, 5);

            // 设备类型图标
            QString icon;
            if (f.type == "light") icon = "💡";
            else if (f.type == "aircon") icon = "❄️";
            else if (f.type == "curtain") icon = "🪟";
            else if (f.type == "camera") icon = "📹";
            else icon = "📱";

            QLabel *iconLabel = new QLabel(icon);
            iconLabel->setStyleSheet("font-size: 20px; min-width: 30px;");
            deviceLayout->addWidget(iconLabel);

            QLabel *nameLabel = new QLabel(f.name);
            nameLabel->setStyleSheet("min-width: 100px;");
            deviceLayout->addWidget(nameLabel);

            QCheckBox *enableCheck = new QCheckBox("启用");
            enableCheck->setStyleSheet("QCheckBox { color: #27ae60; }");
            deviceLayout->addWidget(enableCheck);

            QSlider *valueSlider = nullptr;

            // 根据设备类型添加控制选项
            if (f.type == "light") {
                valueSlider = new QSlider(Qt::Horizontal);
                valueSlider->setRange(0, 100);
                valueSlider->setValue(50);
                valueSlider->setFixedWidth(120);
                valueSlider->setEnabled(false);

                QLabel *valueLabel = new QLabel("50%");
                valueLabel->setFixedWidth(40);

                connect(valueSlider, &QSlider::valueChanged, [=](int v){
                    valueLabel->setText(QString::number(v) + "%");
                });

                connect(enableCheck, &QCheckBox::toggled, valueSlider, &QSlider::setEnabled);
                connect(enableCheck, &QCheckBox::toggled, valueLabel, &QLabel::setEnabled);

                deviceLayout->addWidget(valueSlider);
                deviceLayout->addWidget(valueLabel);
            }
            else if (f.type == "aircon") {
                valueSlider = new QSlider(Qt::Horizontal);
                valueSlider->setRange(16, 30);
                valueSlider->setValue(24);
                valueSlider->setFixedWidth(120);
                valueSlider->setEnabled(false);

                QLabel *valueLabel = new QLabel("24°C");
                valueLabel->setFixedWidth(40);

                connect(valueSlider, &QSlider::valueChanged, [=](int v){
                    valueLabel->setText(QString::number(v) + "°C");
                });

                connect(enableCheck, &QCheckBox::toggled, valueSlider, &QSlider::setEnabled);
                connect(enableCheck, &QCheckBox::toggled, valueLabel, &QLabel::setEnabled);

                deviceLayout->addWidget(valueSlider);
                deviceLayout->addWidget(valueLabel);
            }
            else if (f.type == "curtain") {
                valueSlider = new QSlider(Qt::Horizontal);
                valueSlider->setRange(0, 100);
                valueSlider->setValue(50);
                valueSlider->setFixedWidth(120);
                valueSlider->setEnabled(false);

                QLabel *valueLabel = new QLabel("50%");
                valueLabel->setFixedWidth(40);

                connect(valueSlider, &QSlider::valueChanged, [=](int v){
                    valueLabel->setText(QString::number(v) + "%");
                });

                connect(enableCheck, &QCheckBox::toggled, valueSlider, &QSlider::setEnabled);
                connect(enableCheck, &QCheckBox::toggled, valueLabel, &QLabel::setEnabled);

                deviceLayout->addWidget(valueSlider);
                deviceLayout->addWidget(valueLabel);
            }
            else if (f.type == "camera") {
                valueSlider = new QSlider(Qt::Horizontal);
                valueSlider->setRange(-90, 90);
                valueSlider->setValue(0);
                valueSlider->setFixedWidth(120);
                valueSlider->setEnabled(false);

                QLabel *valueLabel = new QLabel("0°");
                valueLabel->setFixedWidth(40);

                connect(valueSlider, &QSlider::valueChanged, [=](int v){
                    valueLabel->setText(QString::number(v) + "°");
                });

                connect(enableCheck, &QCheckBox::toggled, valueSlider, &QSlider::setEnabled);
                connect(enableCheck, &QCheckBox::toggled, valueLabel, &QLabel::setEnabled);

                deviceLayout->addWidget(valueSlider);
                deviceLayout->addWidget(valueLabel);
            }

            deviceLayout->addStretch();
            roomLayout->addWidget(deviceWidget);

            deviceControls[f.id] = qMakePair(enableCheck, valueSlider);
        }

        scrollLayout->addWidget(roomGroup);
    }

    scrollLayout->addStretch();
    scrollWidget->setLayout(scrollLayout);
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);

    // 按钮
    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    QPushButton *okBtn = new QPushButton("✅ 确定");
    okBtn->setFixedSize(100, 40);
    okBtn->setStyleSheet("QPushButton { background-color: #27ae60; color: white; border-radius: 5px; }"
                         "QPushButton:hover { background-color: #229954; }");

    QPushButton *cancelBtn = new QPushButton("❌ 取消");
    cancelBtn->setFixedSize(100, 40);
    cancelBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; border-radius: 5px; }"
                             "QPushButton:hover { background-color: #c0392b; }");

    btnLayout->addWidget(okBtn);
    btnLayout->addSpacing(20);
    btnLayout->addWidget(cancelBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, [&]() {
        QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(&dialog, "提示", "请输入场景名称");
            return;
        }

        for (const SceneMode &scene : sceneModes) {
            if (scene.name == name) {
                QMessageBox::warning(&dialog, "提示", "场景名称已存在");
                return;
            }
        }

        SceneMode newScene;
        newScene.name = name;

        // 收集所有启用设备的设置
        for (auto it = deviceControls.begin(); it != deviceControls.end(); ++it) {
            int deviceId = it.key();
            QCheckBox *enableCheck = it.value().first;
            QSlider *valueSlider = it.value().second;

            if (enableCheck && enableCheck->isChecked()) {
                newScene.deviceStatus[deviceId] = true;

                if (valueSlider) {
                    newScene.deviceValues[deviceId] = valueSlider->value();
                }
            }
        }

        // 为了兼容旧代码，同时设置客厅设备的值
        Furniture* livingRoomLight = getLivingRoomLight();
        Furniture* livingRoomAircon = getLivingRoomAircon();

        if (livingRoomLight) {
            int id = livingRoomLight->id;
            newScene.lightOn = newScene.deviceStatus.contains(id) ? newScene.deviceStatus[id] : false;
            newScene.lightBright = newScene.deviceValues.contains(id) ? newScene.deviceValues[id] : 50;
        }

        if (livingRoomAircon) {
            int id = livingRoomAircon->id;
            newScene.airconOn = newScene.deviceStatus.contains(id) ? newScene.deviceStatus[id] : false;
            newScene.airconTemp = newScene.deviceValues.contains(id) ? newScene.deviceValues[id] : 24;
        }

        // 查找客厅窗帘和摄像头
        for (const Furniture &f : m_furniture) {
            if (f.type == "curtain" && f.room == "客厅") {
                newScene.curtainOn = newScene.deviceStatus.contains(f.id) ? newScene.deviceStatus[f.id] : false;
                newScene.curtainOpenness = newScene.deviceValues.contains(f.id) ? newScene.deviceValues[f.id] : 50;
            }
            if (f.type == "camera" && f.room == "客厅") {
                newScene.cameraOn = newScene.deviceStatus.contains(f.id) ? newScene.deviceStatus[f.id] : false;
                newScene.cameraAngle = newScene.deviceValues.contains(f.id) ? newScene.deviceValues[f.id] : 0;
            }
        }

        sceneModes.append(newScene);
        saveSceneModes();

        QMessageBox::information(&dialog, "成功", "场景添加成功");
        dialog.accept();
    });

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.exec();
}

void MainWindow::editScene(const QString &name)
{
    int index = -1;
    for (int i = 0; i < sceneModes.size(); i++) {
        if (sceneModes[i].name == name) {
            index = i;
            break;
        }
    }
    if (index == -1) return;

    SceneMode &scene = sceneModes[index];

    QDialog dialog(this);
    dialog.setWindowTitle("修改自定义模式 - " + name);
    dialog.setFixedSize(450, 650);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);

    // 场景名称
    QHBoxLayout *nameLayout = new QHBoxLayout;
    nameLayout->addWidget(new QLabel("场景名称:"));
    QLineEdit *nameEdit = new QLineEdit(scene.name);
    nameLayout->addWidget(nameEdit);
    mainLayout->addLayout(nameLayout);

    // 创建可滚动的设备选择区域
    QLabel *deviceLabel = new QLabel("请选择要控制的设备：");
    deviceLabel->setStyleSheet("font-weight: bold; color: #2c3e50; margin-top: 10px;");
    mainLayout->addWidget(deviceLabel);

    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFixedHeight(450);
    scrollArea->setStyleSheet("QScrollArea { border: 1px solid #bdc3c7; border-radius: 5px; background-color: #f8f9f9; }");

    QWidget *scrollWidget = new QWidget;
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setSpacing(15);

    // 按房间分组显示设备
    QMap<QString, QList<Furniture>> devicesByRoom;
    for (const Furniture &f : m_furniture) {
        devicesByRoom[f.room].append(f);
    }

    QMap<int, QPair<QCheckBox*, QSlider*>> deviceControls;  // 存储每个设备的控件

    // 遍历每个房间
    QStringList roomOrder = {"客厅", "卧室", "厨房", "卫生间", "其他"};
    for (const QString &room : roomOrder) {
        if (!devicesByRoom.contains(room)) continue;

        QGroupBox *roomGroup = new QGroupBox(room);
        roomGroup->setStyleSheet("QGroupBox { font-size: 16px; font-weight: bold; margin-top: 10px; }");
        QVBoxLayout *roomLayout = new QVBoxLayout(roomGroup);
        roomLayout->setSpacing(10);

        QList<Furniture> devices = devicesByRoom[room];
        for (const Furniture &f : devices) {
            QWidget *deviceWidget = new QWidget;
            QHBoxLayout *deviceLayout = new QHBoxLayout(deviceWidget);
            deviceLayout->setContentsMargins(5, 5, 5, 5);

            // 设备类型图标
            QString icon;
            if (f.type == "light") icon = "💡";
            else if (f.type == "aircon") icon = "❄️";
            else if (f.type == "curtain") icon = "🪟";
            else if (f.type == "camera") icon = "📹";
            else icon = "📱";

            QLabel *iconLabel = new QLabel(icon);
            iconLabel->setStyleSheet("font-size: 20px; min-width: 30px;");
            deviceLayout->addWidget(iconLabel);

            QLabel *nameLabel = new QLabel(f.name);
            nameLabel->setStyleSheet("min-width: 100px;");
            deviceLayout->addWidget(nameLabel);

            // 从场景中获取当前设备状态
            bool isEnabled = scene.deviceStatus.contains(f.id) ? scene.deviceStatus[f.id] : false;
            int currentValue = scene.deviceValues.contains(f.id) ? scene.deviceValues[f.id] : 50;

            QCheckBox *enableCheck = new QCheckBox("启用");
            enableCheck->setChecked(isEnabled);
            enableCheck->setStyleSheet("QCheckBox { color: #27ae60; }");
            deviceLayout->addWidget(enableCheck);

            QSlider *valueSlider = nullptr;

            // 根据设备类型添加控制选项
            if (f.type == "light") {
                valueSlider = new QSlider(Qt::Horizontal);
                valueSlider->setRange(0, 100);
                valueSlider->setValue(currentValue);
                valueSlider->setFixedWidth(120);
                valueSlider->setEnabled(isEnabled);

                QLabel *valueLabel = new QLabel(QString::number(currentValue) + "%");
                valueLabel->setFixedWidth(40);
                valueLabel->setEnabled(isEnabled);

                connect(valueSlider, &QSlider::valueChanged, [=](int v){
                    valueLabel->setText(QString::number(v) + "%");
                });

                connect(enableCheck, &QCheckBox::toggled, valueSlider, &QSlider::setEnabled);
                connect(enableCheck, &QCheckBox::toggled, valueLabel, &QLabel::setEnabled);

                deviceLayout->addWidget(valueSlider);
                deviceLayout->addWidget(valueLabel);
            }
            else if (f.type == "aircon") {
                valueSlider = new QSlider(Qt::Horizontal);
                valueSlider->setRange(16, 30);
                valueSlider->setValue(currentValue);
                valueSlider->setFixedWidth(120);
                valueSlider->setEnabled(isEnabled);

                QLabel *valueLabel = new QLabel(QString::number(currentValue) + "°C");
                valueLabel->setFixedWidth(40);
                valueLabel->setEnabled(isEnabled);

                connect(valueSlider, &QSlider::valueChanged, [=](int v){
                    valueLabel->setText(QString::number(v) + "°C");
                });

                connect(enableCheck, &QCheckBox::toggled, valueSlider, &QSlider::setEnabled);
                connect(enableCheck, &QCheckBox::toggled, valueLabel, &QLabel::setEnabled);

                deviceLayout->addWidget(valueSlider);
                deviceLayout->addWidget(valueLabel);
            }
            else if (f.type == "curtain") {
                valueSlider = new QSlider(Qt::Horizontal);
                valueSlider->setRange(0, 100);
                valueSlider->setValue(currentValue);
                valueSlider->setFixedWidth(120);
                valueSlider->setEnabled(isEnabled);

                QLabel *valueLabel = new QLabel(QString::number(currentValue) + "%");
                valueLabel->setFixedWidth(40);
                valueLabel->setEnabled(isEnabled);

                connect(valueSlider, &QSlider::valueChanged, [=](int v){
                    valueLabel->setText(QString::number(v) + "%");
                });

                connect(enableCheck, &QCheckBox::toggled, valueSlider, &QSlider::setEnabled);
                connect(enableCheck, &QCheckBox::toggled, valueLabel, &QLabel::setEnabled);

                deviceLayout->addWidget(valueSlider);
                deviceLayout->addWidget(valueLabel);
            }
            else if (f.type == "camera") {
                valueSlider = new QSlider(Qt::Horizontal);
                valueSlider->setRange(-90, 90);
                valueSlider->setValue(currentValue);
                valueSlider->setFixedWidth(120);
                valueSlider->setEnabled(isEnabled);

                QLabel *valueLabel = new QLabel(QString::number(currentValue) + "°");
                valueLabel->setFixedWidth(40);
                valueLabel->setEnabled(isEnabled);

                connect(valueSlider, &QSlider::valueChanged, [=](int v){
                    valueLabel->setText(QString::number(v) + "°");
                });

                connect(enableCheck, &QCheckBox::toggled, valueSlider, &QSlider::setEnabled);
                connect(enableCheck, &QCheckBox::toggled, valueLabel, &QLabel::setEnabled);

                deviceLayout->addWidget(valueSlider);
                deviceLayout->addWidget(valueLabel);
            }

            deviceLayout->addStretch();
            roomLayout->addWidget(deviceWidget);

            deviceControls[f.id] = qMakePair(enableCheck, valueSlider);
        }

        scrollLayout->addWidget(roomGroup);
    }

    scrollLayout->addStretch();
    scrollWidget->setLayout(scrollLayout);
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);

    // 按钮
    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    QPushButton *okBtn = new QPushButton("✅ 确定");
    okBtn->setFixedSize(100, 40);
    okBtn->setStyleSheet("QPushButton { background-color: #27ae60; color: white; border-radius: 5px; }"
                         "QPushButton:hover { background-color: #229954; }");

    QPushButton *cancelBtn = new QPushButton("❌ 取消");
    cancelBtn->setFixedSize(100, 40);
    cancelBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; border-radius: 5px; }"
                             "QPushButton:hover { background-color: #c0392b; }");

    btnLayout->addWidget(okBtn);
    btnLayout->addSpacing(20);
    btnLayout->addWidget(cancelBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, [&]() {
        QString newName = nameEdit->text().trimmed();
        if (newName.isEmpty()) {
            QMessageBox::warning(&dialog, "提示", "请输入场景名称");
            return;
        }

        for (int i = 0; i < sceneModes.size(); i++) {
            if (i != index && sceneModes[i].name == newName) {
                QMessageBox::warning(&dialog, "提示", "场景名称已存在");
                return;
            }
        }

        // 清空原有的设备状态
        scene.deviceStatus.clear();
        scene.deviceValues.clear();

        // 收集所有启用设备的设置
        for (auto it = deviceControls.begin(); it != deviceControls.end(); ++it) {
            int deviceId = it.key();
            QCheckBox *enableCheck = it.value().first;
            QSlider *valueSlider = it.value().second;

            if (enableCheck && enableCheck->isChecked()) {
                scene.deviceStatus[deviceId] = true;

                if (valueSlider) {
                    scene.deviceValues[deviceId] = valueSlider->value();
                }
            }
        }

        scene.name = newName;

        // 为了兼容旧代码，同时设置客厅设备的值
        Furniture* livingRoomLight = getLivingRoomLight();
        Furniture* livingRoomAircon = getLivingRoomAircon();

        if (livingRoomLight) {
            int id = livingRoomLight->id;
            scene.lightOn = scene.deviceStatus.contains(id) ? scene.deviceStatus[id] : false;
            scene.lightBright = scene.deviceValues.contains(id) ? scene.deviceValues[id] : 50;
        }

        if (livingRoomAircon) {
            int id = livingRoomAircon->id;
            scene.airconOn = scene.deviceStatus.contains(id) ? scene.deviceStatus[id] : false;
            scene.airconTemp = scene.deviceValues.contains(id) ? scene.deviceValues[id] : 24;
        }

        // 查找客厅窗帘和摄像头
        for (const Furniture &f : m_furniture) {
            if (f.type == "curtain" && f.room == "客厅") {
                scene.curtainOn = scene.deviceStatus.contains(f.id) ? scene.deviceStatus[f.id] : false;
                scene.curtainOpenness = scene.deviceValues.contains(f.id) ? scene.deviceValues[f.id] : 50;
            }
            if (f.type == "camera" && f.room == "客厅") {
                scene.cameraOn = scene.deviceStatus.contains(f.id) ? scene.deviceStatus[f.id] : false;
                scene.cameraAngle = scene.deviceValues.contains(f.id) ? scene.deviceValues[f.id] : 0;
            }
        }

        saveSceneModes();

        QMessageBox::information(&dialog, "成功", "场景修改成功");
        dialog.accept();
    });

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.exec();
}

void MainWindow::deleteScene(const QString &name)
{
    if (QMessageBox::question(this, "确认删除", "确定要删除场景 \"" + name + "\" 吗？") == QMessageBox::Yes) {
        for (int i = 0; i < sceneModes.size(); i++) {
            if (sceneModes[i].name == name) {
                sceneModes.removeAt(i);
                saveSceneModes();
                QMessageBox::information(this, "成功", "场景已删除");
                break;
            }
        }
    }
}
// ==================== 错误检查函数 ====================
void MainWindow::checkForErrors()
{
    // 检查其他可能的异常，不重复计数已故障的设备
    for (const Furniture& f : m_furniture) {
        if (f.type == "aircon" && f.status == 1 && f.fault == 0) {
            if (f.temperature < 16 || f.temperature > 30) {
                m_errorDevices++;  // 温度异常，且不是故障设备
            }
        }

        if (f.type == "camera" && f.status == 1 && f.fault == 0) {
            if (f.angle < -90 || f.angle > 90) {
                m_errorDevices++;  // 角度异常
            }
        }
    }
}
// ==================== 首页设备控制 ====================

void MainWindow::controlLivingRoomLight()
{
    Furniture* f = getLivingRoomLight();
    if (f) {
        // 直接调用灯光控制函数，这会弹窗提示并更新状态
        controlLightById(f->id);
    }
}

void MainWindow::controlLivingRoomAircon()
{
    Furniture* f = getLivingRoomAircon();
    if (f) {
        controlAirconById(f->id);
    }
}

// ==================== 场景模式 ====================

void MainWindow::sceneHome()
{
    // 检查是否有设备故障
    QString faultyDevices;
    for (const Furniture &f : m_furniture) {
        if (f.fault == 1) {
            if (!faultyDevices.isEmpty()) faultyDevices += "、";
            faultyDevices += f.name;
        }
    }

    if (!faultyDevices.isEmpty()) {
        QMessageBox::warning(this, "无法执行回家模式",
                             QString("以下设备处于故障状态，无法执行回家模式：\n%1\n请先修复设备。").arg(faultyDevices));
        return;
    }

    // 打开所有设备并设置默认参数
    for (Furniture &f : m_furniture) {
        f.status = 1;  // 开启设备

        // 根据设备类型设置默认参数
        if (f.type == "light") {
            f.brightness = 80;  // 默认亮度80%
        } else if (f.type == "aircon") {
            f.temperature = 26;  // 默认温度26°C
        } else if (f.type == "curtain") {
            f.openness = 100;  // 默认完全打开
        } else if (f.type == "camera") {
            f.angle = 0;  // 默认角度0°
        }

        updateFurnitureInDb(f);
    }

    // 重新加载家具数据
    loadFurnitureFromDb();

    // 更新首页客厅设备状态
    Furniture* livingRoomLight = getLivingRoomLight();
    Furniture* livingRoomAircon = getLivingRoomAircon();

    if (livingRoomLight) {
        livingRoomLightStatusLabel->setText("客厅灯: 开启");
        livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #f39c12;");
        livingRoomLightBrightnessLabel->setText(QString("亮度: %1%").arg(livingRoomLight->brightness));
    }

    if (livingRoomAircon) {
        livingRoomAirconStatusLabel->setText("客厅空调: 开启");
        livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #3498db;");
        livingRoomAirconTempLabel->setText(QString("温度: %1°C").arg(livingRoomAircon->temperature));
    }

    // 构建状态消息
    QString statusMsg = "🏠 回家模式已启动\n\n";
    statusMsg += "已开启所有设备：\n";

    // 按房间统计
    QMap<QString, QStringList> devicesByRoom;
    for (const Furniture &f : m_furniture) {
        QString deviceInfo = f.name;
        if (f.type == "light") {
            deviceInfo += QString(" (亮度:%1%)").arg(f.brightness);
        } else if (f.type == "aircon") {
            deviceInfo += QString(" (温度:%1°C)").arg(f.temperature);
        } else if (f.type == "curtain") {
            deviceInfo += QString(" (开度:%1%)").arg(f.openness);
        } else if (f.type == "camera") {
            deviceInfo += QString(" (角度:%1°)").arg(f.angle);
        }
        devicesByRoom[f.room].append(deviceInfo);
    }

    for (auto it = devicesByRoom.begin(); it != devicesByRoom.end(); ++it) {
        statusMsg += "\n" + it.key() + "：\n";
        for (const QString &device : it.value()) {
            statusMsg += "  • " + device + "\n";
        }
    }

    statusMsg += QString("\n共开启 %1 个设备").arg(m_furniture.size());

    QMessageBox::information(this, "回家模式", statusMsg);
}

void MainWindow::sceneSleep()
{
    // 检查是否有非摄像头设备故障（摄像头故障不影响，因为要保留开启）
    QString faultyDevices;
    for (const Furniture &f : m_furniture) {
        if (f.type == "camera" && f.fault == 1) {
            if (!faultyDevices.isEmpty()) faultyDevices += "、";
            faultyDevices += f.name;
        }
    }

    if (!faultyDevices.isEmpty()) {
        QMessageBox::warning(this, "无法执行睡眠模式",
                             QString("以下设备处于故障状态，无法执行睡眠模式：\n%1\n请先修复设备。").arg(faultyDevices));
        return;
    }

    int cameraCount = 0;
    int otherCount = 0;

    // 遍历所有设备
    for (Furniture &f : m_furniture) {
        if (f.type == "camera") {
            // 摄像头设备保持开启状态
            f.status = 1;

            // 可以设置摄像头为监控角度（比如45°）
            if (f.angle != 45) {
                f.angle = 45;  // 设置一个适合夜间监控的角度
            }

            cameraCount++;
        } else {
            // 非摄像头设备全部关闭
            f.status = 0;

            // 对于窗帘，确保完全关闭
            if (f.type == "curtain") {
                f.openness = 0;
            }

            otherCount++;
        }

        updateFurnitureInDb(f);
    }

    // 重新加载家具数据
    loadFurnitureFromDb();

    // 更新首页客厅设备状态
    Furniture* livingRoomLight = getLivingRoomLight();
    Furniture* livingRoomAircon = getLivingRoomAircon();

    if (livingRoomLight) {
        livingRoomLightStatusLabel->setText("客厅灯: 关闭");
        livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");
        livingRoomLightBrightnessLabel->setText("亮度: --");
    }

    if (livingRoomAircon) {
        livingRoomAirconStatusLabel->setText("客厅空调: 关闭");
        livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");
        livingRoomAirconTempLabel->setText("温度: --");
    }

    // 构建状态消息
    QString statusMsg = "😴 睡眠模式已启动\n\n";

    // 统计开启的摄像头
    QStringList cameraList;
    for (const Furniture &f : m_furniture) {
        if (f.type == "camera" && f.status == 1) {
            cameraList << QString("  • %1 (角度:%2°)").arg(f.name).arg(f.angle);
        }
    }

    if (!cameraList.isEmpty()) {
        statusMsg += "📹 以下摄像头保持开启状态：\n";
        statusMsg += cameraList.join("\n");
        statusMsg += "\n\n";
    }

    statusMsg += "💤 其他设备已全部关闭：\n";

    // 按房间统计已关闭的设备
    QMap<QString, QStringList> closedDevicesByRoom;
    for (const Furniture &f : m_furniture) {
        if (f.type != "camera" && f.status == 0) {
            QString deviceInfo = f.name;
            if (f.type == "curtain") {
                deviceInfo += " (已关闭)";
            }
            closedDevicesByRoom[f.room].append(deviceInfo);
        }
    }

    for (auto it = closedDevicesByRoom.begin(); it != closedDevicesByRoom.end(); ++it) {
        statusMsg += "\n" + it.key() + "：\n";
        for (const QString &device : it.value()) {
            statusMsg += "  • " + device + "\n";
        }
    }

    statusMsg += QString("\n📊 状态统计：\n");
    statusMsg += QString("  • 摄像头运行：%1 个\n").arg(cameraCount);
    statusMsg += QString("  • 其他设备关闭：%1 个\n").arg(otherCount);

    // 添加安全提醒
    statusMsg += "\n🔒 夜间监控已启动，请注意安全！";

    QMessageBox::information(this, "睡眠模式", statusMsg);
}

// ==================== 页面切换函数 ====================

void MainWindow::backToHome()
{
    stackedWidget->setCurrentWidget(homePage);
}

void MainWindow::showAlarm()
{
    stackedWidget->setCurrentWidget(alarmPage);
}

// ==================== 报警相关 ====================

void MainWindow::checkAlarm()
{
    if (!alarmEnabled) return;

    int temp = tempSlider->value();
    int humi = humiSlider->value();
    int light = lightSlider->value();

    // 温度报警
    if (temp < tempMin && !ignoredAlarms.contains("温度过低报警")) {
        addAlarmRecord("温度过低报警", QString("当前温度: %1°C (下限: %2°C)").arg(temp).arg(tempMin), "temperature_low");
    }
    if (temp > tempMax && !ignoredAlarms.contains("温度过高报警")) {
        addAlarmRecord("温度过高报警", QString("当前温度: %1°C (上限: %2°C)").arg(temp).arg(tempMax), "temperature_high");
    }

    // 湿度报警
    if (humi < humiMin && !ignoredAlarms.contains("湿度过低报警")) {
        addAlarmRecord("湿度过低报警", QString("当前湿度: %1% (下限: %2%)").arg(humi).arg(humiMin), "humidity_low");
    }
    if (humi > humiMax && !ignoredAlarms.contains("湿度过高报警")) {
        addAlarmRecord("湿度过高报警", QString("当前湿度: %1% (上限: %2%)").arg(humi).arg(humiMax), "humidity_high");
    }

    // 光照报警
    if (light < lightMin && !ignoredAlarms.contains("光照过低报警")) {
        addAlarmRecord("光照过低报警", QString("当前光照: %1 lux (下限: %2 lux)").arg(light).arg(lightMin), "light_low");
    }
    if (light > lightMax && !ignoredAlarms.contains("光照过高报警")) {
        addAlarmRecord("光照过高报警", QString("当前光照: %1 lux (上限: %2 lux)").arg(light).arg(lightMax), "light_high");
    }
/*
    // 设备故障报警（每分钟一次）
    deviceCheckCounter++;
    if (deviceCheckCounter >= 60) {
        deviceCheckCounter = 0;
        int r = rand() % 3;
        if (r == 0) {
            // 随机选择一个灯光设备故障
            playAlarmSound(true);
            for (Furniture& f : m_furniture) {
                if (f.type == "light") {
                    if (f.fault == 0) {
                        f.fault = 1;
                        updateFurnitureInDb(f);
                        addAlarmRecord("设备报警", f.name + " 故障", "device");

                        // 如果是客厅灯，更新首页状态
                        if (f.room == "客厅") {
                            livingRoomLightStatusLabel->setText("客厅灯: 故障");
                            livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #e74c3c; font-weight: bold;");
                            livingRoomLightBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-size: 14px; border-radius: 5px; }");
                        }
                        break;
                    }
                }
            }
        } else if (r == 1) {
             playAlarmSound(true);
            // 随机选择一个空调设备故障
            for (Furniture& f : m_furniture) {
                if (f.type == "aircon") {
                    if (f.fault == 0) {
                        f.fault = 1;
                        updateFurnitureInDb(f);
                        addAlarmRecord("设备报警", f.name + " 故障", "device");

                        // 如果是客厅空调，更新首页状态
                        if (f.room == "客厅") {
                            livingRoomAirconStatusLabel->setText("客厅空调: 故障");
                            livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #e74c3c; font-weight: bold;");
                            livingRoomAirconBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-size: 14px; border-radius: 5px; }");
                        }
                        break;
                    }
                }
            }
        }

        updateDeviceStats();
    }*/
}

// ==================== 场景模式菜单 ====================

void MainWindow::showCustomModeMenu()
{
    QMenu menu(this);

    if (!sceneModes.isEmpty()) {
        for (const SceneMode &scene : sceneModes) {
            QAction *action = menu.addAction("▶ " + scene.name);
            connect(action, &QAction::triggered, this, [=]() { applySceneMode(scene); });
        }
        menu.addSeparator();
    }

    if (sceneModes.size() < 2) {
        QAction *addAction = menu.addAction("➕ 增加模式 (还可添加 " + QString::number(2 - sceneModes.size()) + " 个)");
        connect(addAction, &QAction::triggered, this, &MainWindow::addNewScene);
    }

    if (!sceneModes.isEmpty()) {
        menu.addSeparator();

        // 修改模式子菜单
        QMenu *editSubMenu = menu.addMenu("✏ 修改模式");
        for (const SceneMode &scene : sceneModes) {
            QAction *action = editSubMenu->addAction(scene.name);
            connect(action, &QAction::triggered, this, [=]() {
                editScene(scene.name);
            });
        }

        // 删除模式子菜单
        QMenu *deleteSubMenu = menu.addMenu("🗑 删除模式");
        for (const SceneMode &scene : sceneModes) {
            QAction *action = deleteSubMenu->addAction(scene.name);
            connect(action, &QAction::triggered, this, [=]() {
                deleteScene(scene.name);
            });
        }
    }

    menu.exec(customModeBtn->mapToGlobal(QPoint(0, customModeBtn->height())));
}
// ==================== 主窗口实现 ====================
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{

    // ===== UI 全局样式优化（不改变原有功能） =====
    qApp->setStyleSheet(R"(
        QPushButton{
            background-color:#3498db;
            color:white;
            border-radius:6px;
            padding:6px;
            font-size:14px;
        }
        QPushButton:hover{
            background-color:#2980b9;
        }
        QGroupBox{
            border:1px solid #dcdcdc;
            border-radius:8px;
            margin-top:10px;
            font-weight:bold;
        }
        QLabel{
            font-size:14px;
        }
        QLineEdit{
            padding:4px;
            border:1px solid #cccccc;
            border-radius:4px;
        }
    )");

    setWindowTitle("智能家居");
    setFixedSize(550, 800);

    initDb();
    initExtensionDb();

    m_deviceCtrl = new DeviceController(this);

    // 初始化报警参数
    tempMin = 18;
    tempMax = 30;
    humiMin = 30;
    humiMax = 70;
    lightMin = 200;
    lightMax = 800;
    alarmEnabled = false;
    deviceCheckCounter = 0;

    lightFault = false;
    airconFault = false;
    faultyDevice = "";

    // 创建修复定时器
    lightFixTimer = new QTimer(this);
    lightFixTimer->setSingleShot(true);
    connect(lightFixTimer, &QTimer::timeout, this, [=]() {
        lightFault = false;
        QMessageBox::information(this, "修复完成", "灯光设备已修复，可以正常使用");
        updateDeviceStats();
    });

    airconFixTimer = new QTimer(this);
    airconFixTimer->setSingleShot(true);
    connect(airconFixTimer, &QTimer::timeout, this, [=]() {
        airconFault = false;
        QMessageBox::information(this, "修复完成", "空调设备已修复，可以正常使用");
        updateDeviceStats();
    });

    m_onlineDevices = 0;
    m_offlineDevices = 0;
    m_errorDevices = 0;

    // 定时生成模拟环境数据
    m_envTimer = new QTimer(this);
    connect(m_envTimer, &QTimer::timeout, this, [=]() {
        EnvData data = generateMockEnvData();
        insertEnvData(data);
    });
    m_envTimer->start(10000);

    stackedWidget = new QStackedWidget(this);
    setCentralWidget(stackedWidget);

    // 创建各个页面
    createLoginPage();
    createRegisterPage();
    createResetPage();
    createHomePage();
    createFurnitureCategoryPage();  // 新分类页
    createFurnitureListPage();      // 新列表页
    createAlarmPage();
    createSysManagePage();

    // 创建报警检查定时器
    alarmCheckTimer = new QTimer(this);
    connect(alarmCheckTimer, &QTimer::timeout, this, &MainWindow::checkAlarm);
    alarmCheckTimer->start(1000);

    // 创建统计刷新定时器
    m_statsTimer = new QTimer(this);
    connect(m_statsTimer, &QTimer::timeout, this, &MainWindow::updateDeviceStats);
    m_statsTimer->start(1000);



    // 从数据库加载家具数据
    loadFurnitureFromDb();

    // 加载自定义场景
    loadSceneModes();
    updateCustomButtons();

    // 默认显示登录页
    stackedWidget->setCurrentWidget(loginPage);

    clientSocket = nullptr; // 初始化为空，避免野指针
    // 1. 初始化 Label 控件（纯代码创建，无需 ui 文件）
    statusLabel = new QLabel("等待指令...", this); // 父对象设为 this，自动管理内存
    statusLabel->setGeometry(20, 20, 200, 30);    // 设置位置和大小（x, y, 宽, 高）
    statusLabel->setStyleSheet("font-size: 16px;"); // 可选：设置样式

    // 2. 初始化 TCP 服务器
    tcpServer = new QTcpServer(this);

    // 3. 监听 8888 端口
    if (!tcpServer->listen(QHostAddress::Any, 8888)) {
        qDebug() << "服务器启动失败：" << tcpServer->errorString();
        statusLabel->setText("启动失败");
    } else {
        qDebug() << "服务器启动成功，监听 8888 端口";
        statusLabel->setText("服务器已启动");
    }
    startHttpServer();
    // 4. 绑定“新客户端连接”信号到槽函数
    connect(tcpServer, &QTcpServer::newConnection, this, &MainWindow::onNewConnection);
}

MainWindow::~MainWindow()
{
    if (m_statsTimer) {
        m_statsTimer->stop();
        delete m_statsTimer;
    }
    if (alarmCheckTimer) {
        alarmCheckTimer->stop();
        delete alarmCheckTimer;
    }
    QSqlDatabase::removeDatabase("qt_sql_default_connection");
    tcpServer->close(); // 关闭服务器
}
// 给手机发送字符串
// 核心：独立的 sendToPhone 函数，无任何外部依赖
void MainWindow::sendToPhone(const QString &msg)
{
    // 第一步：校验客户端套接字是否存在
    if (clientSocket == nullptr)
    {
        qWarning() << "[发送失败] 客户端未连接（套接字为空）";
        // 界面提示（可选，根据你的需求保留）
        statusLabel->setText("发送失败：手机未连接");
        statusLabel->setStyleSheet("font-size: 14px; color: #e74c3c;");
        return;
    }

    // 第二步：校验连接状态
    if (clientSocket->state() != QTcpSocket::ConnectedState)
    {
        qWarning() << "[发送失败] 客户端连接状态异常，当前状态：" << clientSocket->state();
        statusLabel->setText("发送失败：手机已断开连接");
        statusLabel->setStyleSheet("font-size: 14px; color: #e74c3c;");
        // 重置套接字（避免无效连接占用）
        clientSocket->deleteLater();
        clientSocket = nullptr;
        return;
    }

    // 第三步：校验消息是否为空
    QString sendMsg = msg.trimmed();
    if (sendMsg.isEmpty())
    {
        qWarning() << "[发送失败] 消息内容为空";
        QMessageBox::warning(this, "提示", "发送的消息不能为空！");
        return;
    }

    // 第四步：发送消息（UTF8 编码支持中文）
    QByteArray sendData = sendMsg.toUtf8();
    qint64 writeBytes = clientSocket->write(sendData);

    // 立即刷新缓冲区，确保消息发送出去
    clientSocket->flush();

    // 第五步：校验发送结果
    if (writeBytes == -1 || writeBytes != sendData.size())
    {
        qWarning() << "[发送失败] " << clientSocket->errorString();
        statusLabel->setText(QString("发送失败：%1").arg(clientSocket->errorString()));
        statusLabel->setStyleSheet("font-size: 14px; color: #e74c3c;");
        QMessageBox::warning(this, "发送失败", "消息发送失败：" + clientSocket->errorString());
    }
    else
    {
        qInfo() << "[发送成功] 内容：" << sendMsg << " 字节数：" << writeBytes;

    }
}
// 处理新客户端连接
void MainWindow::onNewConnection()
{
    // 第二步：将新连接赋值给「类成员」clientSocket（关键！）
    this->clientSocket = tcpServer->nextPendingConnection();
    if (!this->clientSocket) {
        statusLabel->setText("客户端连接失败：套接字为空");
        return;
    }
    // 获取新连接的套接字
    QTcpSocket *socket = this->clientSocket;
    if (!socket) return;

    qDebug() << "新客户端连接：" << socket->peerAddress().toString();
    statusLabel->setText("客户端已连接");
    sendToPhone("注意：\n1为开启回家模式\n2为开启睡眠模式\n3为开启监控\n4为关闭监控");
    // 绑定套接字的信号到槽函数
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::onDisconnected);
}

// 处理客户端发来的数据
void MainWindow::onReadyRead()
{
    // 获取发送信号的套接字（sender() 就是当前触发的 QTcpSocket）
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray cmd = socket->readAll(); // 读取所有数据
    qDebug() << "收到指令：" << cmd;

    // 根据指令更新状态
    if (cmd == "1") {
        statusLabel->setText("指令：回家模式");
        sceneHome();
        sendToPhone("已开启回家模式");
    } else if (cmd == "2") {
        statusLabel->setText("指令：睡眠模式");
        sceneSleep();
        sendToPhone("已开启睡眠模式");
    } else if (cmd == "3") {
        statusLabel->setText("指令：开客厅监控");
        controlCameraById(7);
        sendToPhone("已开监控");
    } else if (cmd == "4") {
        statusLabel->setText("指令：关客厅监控");
        controlCameraById(7);
        sendToPhone("已关监控");
    } else {
        statusLabel->setText("未知指令：" + cmd);
        sendToPhone("指令不正确，请重新输入\n");
        sendToPhone("注意：\n1为开启回家模式\n2为开启睡眠模式\n3为开启监控\n4为关闭监控");
    }
}

// 处理客户端断开连接
void MainWindow::onDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    qDebug() << "客户端断开连接：" << socket->peerAddress().toString();
    statusLabel->setText("客户端已断开");
    socket->deleteLater(); // 延迟释放套接字资源，避免内存泄漏
}
// ==================== 家具数据库操作 ====================
void MainWindow::loadFurnitureFromDb()
{
    m_furniture.clear();
    QSqlDatabase db = getDbExtensionConnection();
    if (!db.open()) {
        qDebug() << "[家具] 数据库连接失败";
        return;
    }

    QSqlQuery query("SELECT id, type, name, room, status, brightness, temperature, angle, openness, fault FROM furniture", db);
    while (query.next()) {
        Furniture f;
        f.id = query.value(0).toInt();
        f.type = query.value(1).toString();
        f.name = query.value(2).toString();
        f.room = query.value(3).toString();
        f.status = query.value(4).toInt();
        f.brightness = query.value(5).toInt();
        f.temperature = query.value(6).toInt();
        f.angle = query.value(7).toInt();
        f.openness = query.value(8).toInt();
        f.fault = query.value(9).toInt();
        m_furniture.append(f);
    }
    db.close();
}

void MainWindow::updateFurnitureInDb(const Furniture& f)
{
    QSqlDatabase db = getDbExtensionConnection();
    if (!db.open()) {
        qDebug() << "[家具] 数据库连接失败";
        return;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE furniture SET status=?, brightness=?, temperature=?, angle=?, openness=?, fault=? WHERE id=?");
    query.addBindValue(f.status);
    query.addBindValue(f.brightness);
    query.addBindValue(f.temperature);
    query.addBindValue(f.angle);
    query.addBindValue(f.openness);
    query.addBindValue(f.fault);
    query.addBindValue(f.id);
    query.exec();
    db.close();
}

MainWindow::Furniture* MainWindow::getFurnitureById(int id)
{
    for (int i = 0; i < m_furniture.size(); ++i) {
        if (m_furniture[i].id == id)
            return &m_furniture[i];
    }
    return nullptr;
}

QList<MainWindow::Furniture> MainWindow::getFurnitureByType(const QString& type)
{
    QList<Furniture> result;
    foreach (const Furniture& f, m_furniture) {
        if (f.type == type)
            result.append(f);
    }
    return result;
}

MainWindow::Furniture* MainWindow::getLivingRoomLight()
{
    for (Furniture& f : m_furniture) {  // 使用 C++11 范围 for 循环
        if (f.type == "light" && f.room == "客厅")
            return &f;
    }
    return nullptr;
}

MainWindow::Furniture* MainWindow::getLivingRoomAircon()
{
    for (Furniture& f : m_furniture) {
        if (f.type == "aircon" && f.room == "客厅")
            return &f;
    }
    return nullptr;
}
// ==================== 创建登录页 ====================
void MainWindow::createLoginPage()
{
    loginPage = new QWidget;
    QGridLayout *layout = new QGridLayout(loginPage);
    layout->setSpacing(20);
    layout->setContentsMargins(50, 60, 50, 60);

    QLabel *title = new QLabel("🏠 智能家居");
    title->setStyleSheet("font-size: 28px; font-weight: bold; color: #2c3e50;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title, 0, 0, 1, 2);

    layout->setRowMinimumHeight(1, 20);

    QLabel *userLabel = new QLabel("👤 用户名:");
    userLabel->setStyleSheet("font-size: 16px;");
    loginUsername = new QLineEdit;
    loginUsername->setPlaceholderText("请输入用户名");
    loginUsername->setFixedHeight(40);
    loginUsername->setStyleSheet("QLineEdit { border: 2px solid #bdc3c7; border-radius: 5px; padding: 5px; font-size: 14px; }"
                                 "QLineEdit:focus { border-color: #3498db; }");

    layout->addWidget(userLabel, 2, 0, Qt::AlignRight);
    layout->addWidget(loginUsername, 2, 1);

    QLabel *pwdLabel = new QLabel("🔒 密码:");
    pwdLabel->setStyleSheet("font-size: 16px;");
    loginPassword = new QLineEdit;
    loginPassword->setPlaceholderText("请输入密码");
    loginPassword->setEchoMode(QLineEdit::Password);
    loginPassword->setFixedHeight(40);
    loginPassword->setStyleSheet("QLineEdit { border: 2px solid #bdc3c7; border-radius: 5px; padding: 5px; font-size: 14px; }"
                                 "QLineEdit:focus { border-color: #3498db; }");

    layout->addWidget(pwdLabel, 3, 0, Qt::AlignRight);
    layout->addWidget(loginPassword, 3, 1);

    layout->setRowMinimumHeight(4, 20);

    loginBtn = new QPushButton("🔑 登录");
    loginBtn->setFixedHeight(45);
    loginBtn->setStyleSheet("QPushButton { background-color: #3498db; color: white; font-size: 16px; border-radius: 5px; }"
                            "QPushButton:hover { background-color: #2980b9; }");
    layout->addWidget(loginBtn, 5, 0, 1, 2);

    layout->setRowMinimumHeight(6, 10);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(15);

    registerBtn = new QPushButton("📝 注册新账号");
    registerBtn->setFixedHeight(40);
    registerBtn->setStyleSheet("QPushButton { background-color: #2ecc71; color: white; font-size: 14px; border-radius: 5px; }"
                               "QPushButton:hover { background-color: #27ae60; }");

    resetBtn = new QPushButton("❓ 忘记密码");
    resetBtn->setFixedHeight(40);
    resetBtn->setStyleSheet("QPushButton { background-color: #e67e22; color: white; font-size: 14px; border-radius: 5px; }"
                            "QPushButton:hover { background-color: #d35400; }");

    btnLayout->addWidget(registerBtn);
    btnLayout->addWidget(resetBtn);

    layout->addLayout(btnLayout, 7, 0, 1, 2);

    QLabel *copyright = new QLabel("© 2026 智能家居控制系统 v1.0");
    copyright->setStyleSheet("color: #95a5a6; font-size: 12px;");
    copyright->setAlignment(Qt::AlignCenter);
    layout->addWidget(copyright, 8, 0, 1, 2);

    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 2);

    connect(loginBtn, &QPushButton::clicked, this, &MainWindow::doLogin);
    connect(registerBtn, &QPushButton::clicked, this, &MainWindow::showRegister);
    connect(resetBtn, &QPushButton::clicked, this, &MainWindow::showReset);
    connect(loginPassword, &QLineEdit::returnPressed, this, &MainWindow::doLogin);

    stackedWidget->addWidget(loginPage);
}

// ==================== 创建注册页 ====================
void MainWindow::createRegisterPage()
{
    registerPage = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(registerPage);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    mainLayout->addStretch();

    QHBoxLayout *centerLayout = new QHBoxLayout;
    centerLayout->addStretch();

    QVBoxLayout *contentLayout = new QVBoxLayout;
    contentLayout->setSpacing(30);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *title = new QLabel("📝 用户注册");
    title->setStyleSheet("font-size: 28px; font-weight: bold; color: #2c3e50;");
    title->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(title);

    QGridLayout *layout = new QGridLayout;
    layout->setSpacing(20);
    layout->setColumnMinimumWidth(0, 120);

    QLabel *userLabel = new QLabel("👤 用户名:");
    userLabel->setStyleSheet("font-size: 16px;");
    regUsername = new QLineEdit;
    regUsername->setPlaceholderText("请输入用户名");
    regUsername->setFixedSize(250, 40);
    regUsername->setStyleSheet("QLineEdit { border: 2px solid #bdc3c7; border-radius: 5px; padding: 5px; font-size: 14px; }"
                               "QLineEdit:focus { border-color: #2ecc71; }");

    layout->addWidget(userLabel, 0, 0, Qt::AlignRight);
    layout->addWidget(regUsername, 0, 1);

    QLabel *pwdLabel = new QLabel("🔒 密码:");
    pwdLabel->setStyleSheet("font-size: 16px;");
    regPassword = new QLineEdit;
    regPassword->setPlaceholderText("请输入密码");
    regPassword->setEchoMode(QLineEdit::Password);
    regPassword->setFixedSize(250, 40);
    regPassword->setStyleSheet("QLineEdit { border: 2px solid #bdc3c7; border-radius: 5px; padding: 5px; font-size: 14px; }"
                               "QLineEdit:focus { border-color: #2ecc71; }");

    layout->addWidget(pwdLabel, 1, 0, Qt::AlignRight);
    layout->addWidget(regPassword, 1, 1);

    QLabel *confirmLabel = new QLabel("✓ 确认密码:");
    confirmLabel->setStyleSheet("font-size: 16px;");
    regConfirmPassword = new QLineEdit;
    regConfirmPassword->setPlaceholderText("请再次输入密码");
    regConfirmPassword->setEchoMode(QLineEdit::Password);
    regConfirmPassword->setFixedSize(250, 40);
    regConfirmPassword->setStyleSheet("QLineEdit { border: 2px solid #bdc3c7; border-radius: 5px; padding: 5px; font-size: 14px; }"
                                      "QLineEdit:focus { border-color: #2ecc71; }");

    layout->addWidget(confirmLabel, 2, 0, Qt::AlignRight);
    layout->addWidget(regConfirmPassword, 2, 1);

    contentLayout->addLayout(layout);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    regConfirmBtn = new QPushButton("✅ 注册");
    regConfirmBtn->setFixedSize(120, 45);
    regConfirmBtn->setStyleSheet("QPushButton { background-color: #2ecc71; color: white; font-size: 16px; border-radius: 5px; }"
                                 "QPushButton:hover { background-color: #27ae60; }");

    regBackBtn = new QPushButton("◀ 返回");
    regBackBtn->setFixedSize(100, 40);
    regBackBtn->setStyleSheet("QPushButton { background-color: #95a5a6; color: white; font-size: 14px; border-radius: 5px; }"
                              "QPushButton:hover { background-color: #7f8c8d; }");

    btnLayout->addWidget(regConfirmBtn);
    btnLayout->addSpacing(20);
    btnLayout->addWidget(regBackBtn);
    btnLayout->addStretch();

    contentLayout->addLayout(btnLayout);

    centerLayout->addLayout(contentLayout);
    centerLayout->addStretch();

    mainLayout->addLayout(centerLayout);
    mainLayout->addStretch();

    connect(regConfirmBtn, &QPushButton::clicked, this, &MainWindow::doRegister);
    connect(regBackBtn, &QPushButton::clicked, this, &MainWindow::backToLogin);

    stackedWidget->addWidget(registerPage);
}

// ==================== 创建重置密码页 ====================
void MainWindow::createResetPage()
{
    resetPage = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(resetPage);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    mainLayout->addStretch();

    QHBoxLayout *centerLayout = new QHBoxLayout;
    centerLayout->addStretch();

    QVBoxLayout *contentLayout = new QVBoxLayout;
    contentLayout->setSpacing(30);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *title = new QLabel("🔄 重置密码");
    title->setStyleSheet("font-size: 28px; font-weight: bold; color: #2c3e50;");
    title->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(title);

    QGridLayout *layout = new QGridLayout;
    layout->setSpacing(20);
    layout->setColumnMinimumWidth(0, 120);

    QLabel *userLabel = new QLabel("👤 用户名:");
    userLabel->setStyleSheet("font-size: 16px;");
    resetUsername = new QLineEdit;
    resetUsername->setPlaceholderText("请输入用户名");
    resetUsername->setFixedSize(250, 40);
    resetUsername->setStyleSheet("QLineEdit { border: 2px solid #bdc3c7; border-radius: 5px; padding: 5px; font-size: 14px; }"
                                 "QLineEdit:focus { border-color: #e67e22; }");

    layout->addWidget(userLabel, 0, 0, Qt::AlignRight);
    layout->addWidget(resetUsername, 0, 1);

    QLabel *pwdLabel = new QLabel("🔒 新密码:");
    pwdLabel->setStyleSheet("font-size: 16px;");
    resetNewPassword = new QLineEdit;
    resetNewPassword->setPlaceholderText("请输入新密码");
    resetNewPassword->setEchoMode(QLineEdit::Password);
    resetNewPassword->setFixedSize(250, 40);
    resetNewPassword->setStyleSheet("QLineEdit { border: 2px solid #bdc3c7; border-radius: 5px; padding: 5px; font-size: 14px; }"
                                    "QLineEdit:focus { border-color: #e67e22; }");

    layout->addWidget(pwdLabel, 1, 0, Qt::AlignRight);
    layout->addWidget(resetNewPassword, 1, 1);

    QLabel *confirmLabel = new QLabel("✓ 确认密码:");
    confirmLabel->setStyleSheet("font-size: 16px;");
    resetConfirmPassword = new QLineEdit;
    resetConfirmPassword->setPlaceholderText("请再次输入新密码");
    resetConfirmPassword->setEchoMode(QLineEdit::Password);
    resetConfirmPassword->setFixedSize(250, 40);
    resetConfirmPassword->setStyleSheet("QLineEdit { border: 2px solid #bdc3c7; border-radius: 5px; padding: 5px; font-size: 14px; }"
                                        "QLineEdit:focus { border-color: #e67e22; }");

    layout->addWidget(confirmLabel, 2, 0, Qt::AlignRight);
    layout->addWidget(resetConfirmPassword, 2, 1);

    contentLayout->addLayout(layout);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    resetConfirmBtn = new QPushButton("🔄 重置");
    resetConfirmBtn->setFixedSize(120, 45);
    resetConfirmBtn->setStyleSheet("QPushButton { background-color: #e67e22; color: white; font-size: 16px; border-radius: 5px; }"
                                   "QPushButton:hover { background-color: #d35400; }");

    resetBackBtn = new QPushButton("◀ 返回");
    resetBackBtn->setFixedSize(100, 40);
    resetBackBtn->setStyleSheet("QPushButton { background-color: #95a5a6; color: white; font-size: 14px; border-radius: 5px; }"
                                "QPushButton:hover { background-color: #7f8c8d; }");

    btnLayout->addWidget(resetConfirmBtn);
    btnLayout->addSpacing(20);
    btnLayout->addWidget(resetBackBtn);
    btnLayout->addStretch();

    contentLayout->addLayout(btnLayout);

    centerLayout->addLayout(contentLayout);
    centerLayout->addStretch();

    mainLayout->addLayout(centerLayout);
    mainLayout->addStretch();

    connect(resetConfirmBtn, &QPushButton::clicked, this, &MainWindow::doReset);
    connect(resetBackBtn, &QPushButton::clicked, this, &MainWindow::backToLogin);

    stackedWidget->addWidget(resetPage);
}

// ==================== 创建首页 ====================
void MainWindow::createHomePage()
{
    homePage = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(homePage);
    layout->setSpacing(15);
    layout->setContentsMargins(20, 20, 20, 20);

    welcomeLabel = new QLabel("欢迎使用智能家居");
    welcomeLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    welcomeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(welcomeLabel);

    // ========== 设备总览 ==========
    QGroupBox *group1 = new QGroupBox("设备总览");
    QVBoxLayout *v1 = new QVBoxLayout(group1);

    QHBoxLayout *statsLayout = new QHBoxLayout;
    statsLayout->setSpacing(10);

    m_onlineLabel = new QLabel("在线设备: 0");
    m_onlineLabel->setAlignment(Qt::AlignCenter);
    m_onlineLabel->setStyleSheet("QLabel { color: #27ae60; font-size: 14px; padding: 8px; border: 1px solid #bdc3c7; border-radius: 5px; background-color: #f8f9f9; }"
                                 "QLabel:hover { background-color: #e8f8f5; border-color: #27ae60; }");
    m_onlineLabel->setCursor(Qt::PointingHandCursor);
    m_onlineLabel->setToolTip("点击查看在线设备列表");

    m_offlineLabel = new QLabel("离线设备: 0");
    m_offlineLabel->setAlignment(Qt::AlignCenter);
    m_offlineLabel->setStyleSheet("QLabel { color: #e67e22; font-size: 14px; padding: 8px; border: 1px solid #bdc3c7; border-radius: 5px; background-color: #f8f9f9; }"
                                  "QLabel:hover { background-color: #fef5e7; border-color: #e67e22; }");
    m_offlineLabel->setCursor(Qt::PointingHandCursor);
    m_offlineLabel->setToolTip("点击查看离线设备列表");

    m_errorLabel = new QLabel("异常设备: 0");
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setStyleSheet("QLabel { color: #e74c3c; font-size: 14px; padding: 8px; border: 1px solid #bdc3c7; border-radius: 5px; background-color: #f8f9f9; }"
                                "QLabel:hover { background-color: #fdedec; border-color: #e74c3c; }");
    m_errorLabel->setCursor(Qt::PointingHandCursor);
    m_errorLabel->setToolTip("点击查看异常设备列表");

    statsLayout->addWidget(m_onlineLabel);
    statsLayout->addWidget(m_offlineLabel);
    statsLayout->addWidget(m_errorLabel);
    v1->addLayout(statsLayout);

    alarmEntryBtn = new QPushButton("🚨 查看报警");
    alarmEntryBtn->setFixedSize(150, 35);
    alarmEntryBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; border-radius: 5px; }"
                                 "QPushButton:hover { background-color: #c0392b; }");
    v1->addWidget(alarmEntryBtn, 0, Qt::AlignCenter);

    layout->addWidget(group1);

    // ========== 常用设备（客厅灯和客厅空调） ==========
    QGroupBox *group2 = new QGroupBox("常用设备");
    QVBoxLayout *v2 = new QVBoxLayout(group2);
    v2->setSpacing(10);

    // 客厅灯
    QHBoxLayout *lightInfoLayout = new QHBoxLayout;
    lightInfoLayout->addWidget(new QLabel("💡"));
    livingRoomLightStatusLabel = new QLabel("客厅灯: 关闭");
    livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");
    lightInfoLayout->addWidget(livingRoomLightStatusLabel);
    lightInfoLayout->addStretch();
    livingRoomLightBrightnessLabel = new QLabel("亮度: 80%");
    livingRoomLightBrightnessLabel->setStyleSheet("font-size: 14px; color: #f39c12;");
    lightInfoLayout->addWidget(livingRoomLightBrightnessLabel);
    v2->addLayout(lightInfoLayout);
    livingRoomLightBtn = new QPushButton("控制客厅灯");
    livingRoomLightBtn->setFixedHeight(40);
    livingRoomLightBtn->setStyleSheet("QPushButton { background-color: #f1c40f; font-size: 14px; border-radius: 5px; }");
    v2->addWidget(livingRoomLightBtn);

    // 客厅空调
    QHBoxLayout *airconInfoLayout = new QHBoxLayout;
    airconInfoLayout->addWidget(new QLabel("❄️"));
    livingRoomAirconStatusLabel = new QLabel("客厅空调: 关闭");
    livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");
    airconInfoLayout->addWidget(livingRoomAirconStatusLabel);
    airconInfoLayout->addStretch();
    livingRoomAirconTempLabel = new QLabel("温度: 26°C");
    livingRoomAirconTempLabel->setStyleSheet("font-size: 14px; color: #3498db;");
    airconInfoLayout->addWidget(livingRoomAirconTempLabel);
    v2->addLayout(airconInfoLayout);
    livingRoomAirconBtn = new QPushButton("控制客厅空调");
    livingRoomAirconBtn->setFixedHeight(40);
    livingRoomAirconBtn->setStyleSheet("QPushButton { background-color: #3498db; color: white; font-size: 14px; border-radius: 5px; }");
    v2->addWidget(livingRoomAirconBtn);

    // 更多设备按钮
    QPushButton *moreBtn = new QPushButton("📱 更多设备 >>");
    moreBtn->setFixedHeight(45);
    moreBtn->setStyleSheet("QPushButton { background-color: #9b59b6; color: white; font-size: 15px; border-radius: 5px; }"
                           "QPushButton:hover { background-color: #8e44ad; }");
    v2->addWidget(moreBtn);

    layout->addWidget(group2);

    // ========== 场景模式 ==========
    QGroupBox *group3 = new QGroupBox("场景模式");
    QGridLayout *grid = new QGridLayout(group3);
    grid->setSpacing(10);

    homeModeBtn = new QPushButton("回家模式");
    homeModeBtn->setFixedSize(100, 50);
    homeModeBtn->setStyleSheet("QPushButton { background-color: #27ae60; color: white; border-radius: 5px; }");

    sleepModeBtn = new QPushButton("睡眠模式");
    sleepModeBtn->setFixedSize(100, 50);
    sleepModeBtn->setStyleSheet("QPushButton { background-color: #2980b9; color: white; border-radius: 5px; }");

    custom1Btn = new QPushButton("未定义一");
    custom1Btn->setFixedSize(100, 40);
    custom1Btn->setStyleSheet("QPushButton { background-color: #95a5a6; color: white; border-radius: 5px; }");

    custom2Btn = new QPushButton("未定义二");
    custom2Btn->setFixedSize(100, 40);
    custom2Btn->setStyleSheet("QPushButton { background-color: #95a5a6; color: white; border-radius: 5px; }");

    customModeBtn = new QPushButton("⚙ 管理");
    customModeBtn->setFixedSize(100, 40);
    customModeBtn->setStyleSheet("QPushButton { background-color: #e67e22; color: white; border-radius: 5px; }");

    grid->addWidget(homeModeBtn, 0, 0);
    grid->addWidget(sleepModeBtn, 0, 1);
    grid->addWidget(custom1Btn, 1, 0);
    grid->addWidget(custom2Btn, 1, 1);
    grid->addWidget(customModeBtn, 1, 2);

    layout->addWidget(group3);

    // ========== 系统管理 ==========
    QPushButton *sysManageBtn = new QPushButton("⚙️ 系统管理");
    sysManageBtn->setFixedSize(120, 50);
    sysManageBtn->setStyleSheet("QPushButton { background-color: #34495e; color: white; font-size: 14px; border-radius: 5px; }");
    layout->addWidget(sysManageBtn, 0, Qt::AlignCenter);

    connect(sysManageBtn, &QPushButton::clicked, this, [=]() {
        loadDeviceList();
        stackedWidget->setCurrentWidget(sysManagePage);
    });

    // ========== 数据中心按钮 ==========
    QPushButton* dataCenterBtn = new QPushButton("📊 数据中心");
    dataCenterBtn->setFixedSize(120, 40);
    dataCenterBtn->setStyleSheet("QPushButton { background-color: #2ecc71; color: white; border-radius: 5px; font-size: 14px; }"
                                 "QPushButton:hover { background-color: #27ae60; }");
    layout->addWidget(dataCenterBtn, 0, Qt::AlignCenter);

    // ========== 退出按钮 ==========
    logoutBtn = new QPushButton("退出登录");
    logoutBtn->setFixedSize(120, 40);
    logoutBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; border-radius: 5px; }");
    layout->addWidget(logoutBtn, 0, Qt::AlignCenter);

    // ========== 连接信号 ==========
    connect(livingRoomLightBtn, &QPushButton::clicked, this, &MainWindow::controlLivingRoomLight);
    connect(livingRoomAirconBtn, &QPushButton::clicked, this, &MainWindow::controlLivingRoomAircon);
    connect(moreBtn, &QPushButton::clicked, this, &MainWindow::showFurnitureCategory);
    connect(alarmEntryBtn, &QPushButton::clicked, this, &MainWindow::showAlarm);
    connect(logoutBtn, &QPushButton::clicked, this, &MainWindow::doLogout);

    // 场景模式连接
    connect(homeModeBtn, &QPushButton::clicked, this, &MainWindow::sceneHome);
    connect(sleepModeBtn, &QPushButton::clicked, this, &MainWindow::sceneSleep);

    connect(custom1Btn, &QPushButton::clicked, this, [=]() {
        if (sceneModes.size() > 0) applySceneMode(sceneModes[0]);
        else QMessageBox::information(this, "提示", "当前按钮未定义，请先添加自定义模式");
    });

    connect(custom2Btn, &QPushButton::clicked, this, [=]() {
        if (sceneModes.size() > 1) applySceneMode(sceneModes[1]);
        else QMessageBox::information(this, "提示", "当前按钮未定义，请先添加自定义模式");
    });

    connect(customModeBtn, &QPushButton::clicked, this, &MainWindow::showCustomModeMenu);

    // 数据中心按钮连接
    connect(dataCenterBtn, &QPushButton::clicked, this, [=]() {
        QString safeUsername = m_currentUser.isEmpty() ? "admin" : m_currentUser;
        RecordWindow *recordWindow = new RecordWindow(safeUsername, this);
        recordWindow->setAttribute(Qt::WA_DeleteOnClose);
        recordWindow->exec();
    });

    // 安装事件过滤器
    m_onlineLabel->installEventFilter(this);
    m_offlineLabel->installEventFilter(this);
    m_errorLabel->installEventFilter(this);

    stackedWidget->addWidget(homePage);
}
// ==================== 创建家具分类页 ====================
void MainWindow::createFurnitureCategoryPage()
{
    furnitureCategoryPage = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(furnitureCategoryPage);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(40, 30, 40, 30);

    QLabel *title = new QLabel("📱 设备分类");
    title->setStyleSheet("font-size: 22px; font-weight: bold; color: #2c3e50;");
    title->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title);

    // 灯光按钮
    lightCategoryBtn = new QPushButton("💡 灯光设备");
    lightCategoryBtn->setFixedSize(200, 80);
    lightCategoryBtn->setStyleSheet("QPushButton { background-color: #f1c40f; color: #2c3e50; font-size: 18px; font-weight: bold; border-radius: 10px; }"
                                    "QPushButton:hover { background-color: #f39c12; }");
    mainLayout->addWidget(lightCategoryBtn, 0, Qt::AlignCenter);
    qDebug() << "灯光按钮创建完成:" << lightCategoryBtn;

    // 空调按钮
    airconCategoryBtn = new QPushButton("❄️ 空调设备");
    airconCategoryBtn->setFixedSize(200, 80);
    airconCategoryBtn->setStyleSheet("QPushButton { background-color: #3498db; color: white; font-size: 18px; font-weight: bold; border-radius: 10px; }"
                                     "QPushButton:hover { background-color: #2980b9; }");
    mainLayout->addWidget(airconCategoryBtn, 0, Qt::AlignCenter);

    // 摄像头按钮
    cameraCategoryBtn = new QPushButton("📹 摄像头设备");
    cameraCategoryBtn->setFixedSize(200, 80);
    cameraCategoryBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-size: 18px; font-weight: bold; border-radius: 10px; }"
                                     "QPushButton:hover { background-color: #c0392b; }");
    mainLayout->addWidget(cameraCategoryBtn, 0, Qt::AlignCenter);

    // 窗帘按钮
    curtainCategoryBtn = new QPushButton("🪟 窗帘设备");
    curtainCategoryBtn->setFixedSize(200, 80);
    curtainCategoryBtn->setStyleSheet("QPushButton { background-color: #9b59b6; color: white; font-size: 18px; font-weight: bold; border-radius: 10px; }"
                                      "QPushButton:hover { background-color: #8e44ad; }");
    mainLayout->addWidget(curtainCategoryBtn, 0, Qt::AlignCenter);

    // 返回按钮
    backFromCategoryBtn = new QPushButton("🏠 返回首页");
    backFromCategoryBtn->setFixedSize(220, 50);
    backFromCategoryBtn->setStyleSheet("QPushButton { background-color: #95a5a6; color: white; font-size: 16px; border-radius: 5px; }"
                                       "QPushButton:hover { background-color: #7f8c8d; }");
    mainLayout->addWidget(backFromCategoryBtn, 0, Qt::AlignCenter);

    mainLayout->addStretch();

    connect(lightCategoryBtn, &QPushButton::clicked, this, [=]() { showFurnitureList("light"); });
    connect(airconCategoryBtn, &QPushButton::clicked, this, [=]() { showFurnitureList("aircon"); });
    connect(cameraCategoryBtn, &QPushButton::clicked, this, [=]() { showFurnitureList("camera"); });
    connect(curtainCategoryBtn, &QPushButton::clicked, this, [=]() { showFurnitureList("curtain"); });
    connect(backFromCategoryBtn, &QPushButton::clicked, this, &MainWindow::backToHome);

    stackedWidget->addWidget(furnitureCategoryPage);
}

// ==================== 创建家具列表页 ====================
void MainWindow::createFurnitureListPage()
{
    furnitureListPage = new QWidget;

    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(furnitureListPage);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // 创建滚动区域（保存为成员变量）
    furnitureScrollArea = new QScrollArea;
    furnitureScrollArea->setWidgetResizable(true);
    furnitureScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    furnitureScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    furnitureScrollArea->setFrameShape(QFrame::NoFrame);

    // 创建滚动区域内部的内容容器
    QWidget *scrollContent = new QWidget;
    furnitureListLayout = new QVBoxLayout(scrollContent);
    furnitureListLayout->setSpacing(15);
    furnitureListLayout->setContentsMargins(10, 10, 10, 10);
    furnitureListLayout->setAlignment(Qt::AlignTop);

    furnitureScrollArea->setWidget(scrollContent);

    // 返回按钮
    backFromListBtn = new QPushButton("◀ 返回分类");
    backFromListBtn->setFixedSize(200, 45);
    backFromListBtn->setStyleSheet(
        "QPushButton { background-color:#95a5a6; color:white; font-size:16px; border-radius:5px; }"
        "QPushButton:hover { background-color:#7f8c8d; }"
        );

    mainLayout->addWidget(backFromListBtn, 0, Qt::AlignCenter);
    mainLayout->addWidget(furnitureScrollArea, 1);

    connect(backFromListBtn, &QPushButton::clicked, this, &MainWindow::backToCategory);

    stackedWidget->addWidget(furnitureListPage);
}
void MainWindow::showFurnitureCategory()
{
    stackedWidget->setCurrentWidget(furnitureCategoryPage);
}
void MainWindow::showFurnitureList(const QString& category)
{
    m_currentCategory = category;

    // 清除旧的控件
    for (QWidget* w : furnitureItemWidgets) {
        w->deleteLater();
    }
    furnitureItemWidgets.clear();

    // 移除旧的布局项
    QLayoutItem *item;
    while ((item = furnitureListLayout->takeAt(0)) != nullptr) {
        delete item;
    }

    // 获取该分类的所有家具
    QList<Furniture> items = getFurnitureByType(category);

    QString categoryTitle;
    if (category == "light") categoryTitle = "💡 灯光设备列表";
    else if (category == "aircon") categoryTitle = "❄️ 空调设备列表";
    else if (category == "camera") categoryTitle = "📹 摄像头设备列表";
    else if (category == "curtain") categoryTitle = "🪟 窗帘设备列表";

    QLabel *titleLabel = new QLabel(categoryTitle);
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #2c3e50; margin: 10px 0;");
    titleLabel->setAlignment(Qt::AlignCenter);
    furnitureListLayout->addWidget(titleLabel);
    furnitureItemWidgets.append(titleLabel);

    // 创建网格布局来放置设备卡片
    QGridLayout *gridLayout = new QGridLayout;
    gridLayout->setSpacing(15);
    gridLayout->setContentsMargins(0, 10, 0, 10);

    int row = 0;
    int col = 0;
    const int COLS = 2;  // 每行显示2个设备

    foreach (const Furniture& f, items) {
        QGroupBox *groupBox = new QGroupBox(f.name);

        // 如果设备故障，设置红色边框
        if (f.fault == 1) {
            groupBox->setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; margin: 5px; border: 2px solid #e74c3c; }");
        } else {
            groupBox->setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; margin: 5px; }");
        }
        groupBox->setFixedSize(260, 220);  // 增加高度以容纳修复按钮

        if (category == "light") {
            // ========== 灯光控制界面 ==========
            QVBoxLayout *layout = new QVBoxLayout(groupBox);
            layout->setSpacing(8);

            // 状态标签
            QString statusText;
            QString statusColor;
            if (f.fault == 1) {
                statusText = "状态: 故障";
                statusColor = "#e74c3c";
            } else {
                statusText = f.status ? "状态: 开启" : "状态: 关闭";
                statusColor = f.status ? "#f39c12" : "#7f8c8d";
            }

            QLabel *statusLabel = new QLabel(statusText);
            statusLabel->setStyleSheet(QString("font-size: 13px; color: %1;").arg(statusColor));

            // 开关按钮
            QPushButton *toggleBtn = new QPushButton(f.status ? "关闭灯光" : "开启灯光");
            toggleBtn->setFixedSize(120, 35);

            if (f.fault == 1) {
                toggleBtn->setEnabled(false);
                toggleBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; border-radius: 5px; }");
            } else {
                toggleBtn->setStyleSheet("QPushButton { background-color: #f1c40f; color: #2c3e50; font-weight: bold; border-radius: 5px; }"
                                         "QPushButton:hover { background-color: #f39c12; }");
            }

            // 修复按钮（仅故障时显示）
            QPushButton *fixBtn = nullptr;
            if (f.fault == 1) {
                fixBtn = new QPushButton("🔧 修复设备");
                fixBtn->setFixedSize(120, 30);
                fixBtn->setStyleSheet("QPushButton { background-color: #27ae60; color: white; font-weight: bold; border-radius: 5px; }"
                                      "QPushButton:hover { background-color: #229954; }");
                layout->addWidget(fixBtn, 0, Qt::AlignCenter);
            }

            QHBoxLayout *brightLayout = new QHBoxLayout;
            QLabel *brightLabel = new QLabel(QString("亮度: %1%").arg(f.brightness));
            brightLabel->setStyleSheet(f.fault == 1 ? "font-size: 13px; color: #e74c3c;" : "font-size: 13px; color: #f39c12; font-weight: bold;");

            QPushButton *brightDownBtn = new QPushButton("-");
            QPushButton *brightUpBtn = new QPushButton("+");
            brightDownBtn->setFixedSize(35, 30);
            brightUpBtn->setFixedSize(35, 30);

            if (f.fault == 1 || !f.status) {
                brightDownBtn->setEnabled(false);
                brightUpBtn->setEnabled(false);
            }

            brightDownBtn->setStyleSheet(f.fault == 1 ? "QPushButton { background-color: #e74c3c; color: white; font-size: 16px; font-weight: bold; border-radius: 5px; }"
                                                      : "QPushButton { background-color: #f1c40f; color: #2c3e50; font-size: 16px; font-weight: bold; border-radius: 5px; }");
            brightUpBtn->setStyleSheet(f.fault == 1 ? "QPushButton { background-color: #e74c3c; color: white; font-size: 16px; font-weight: bold; border-radius: 5px; }"
                                                    : "QPushButton { background-color: #f1c40f; color: #2c3e50; font-size: 16px; font-weight: bold; border-radius: 5px; }");

            brightLayout->addWidget(brightLabel);
            brightLayout->addWidget(brightDownBtn);
            brightLayout->addWidget(brightUpBtn);

            layout->addWidget(statusLabel, 0, Qt::AlignCenter);
            layout->addWidget(toggleBtn, 0, Qt::AlignCenter);
            layout->addLayout(brightLayout);

            int id = f.id;
            if (f.fault == 0) {
                connect(toggleBtn, &QPushButton::clicked, this, [=]() {
                    controlLightById(id);
                    QTimer::singleShot(100, this, [=]() {
                        showFurnitureList(category);
                    });
                });
                connect(brightUpBtn, &QPushButton::clicked, this, [=]() {
                    brightnessUpById(id);
                    QTimer::singleShot(100, this, [=]() {
                        showFurnitureList(category);
                    });
                });
                connect(brightDownBtn, &QPushButton::clicked, this, [=]() {
                    brightnessDownById(id);
                    QTimer::singleShot(100, this, [=]() {
                        showFurnitureList(category);
                    });
                });
            }

            if (fixBtn) {
                connect(fixBtn, &QPushButton::clicked, this, [=]() {
                    QMessageBox msgBox;
                    msgBox.setWindowTitle("⚠️ 设备故障");
                    msgBox.setText(QString("%1当前处于故障状态，是否立即修复？").arg(f.name));
                    msgBox.setIcon(QMessageBox::Question);

                    QPushButton *fixActionBtn = msgBox.addButton("立即修复（5秒）", QMessageBox::ActionRole);
                    QPushButton *cancelBtn = msgBox.addButton("取消", QMessageBox::RejectRole);

                    msgBox.exec();

                    if (msgBox.clickedButton() == fixActionBtn) {
                        QMessageBox::information(this, "修复中", QString("正在修复%1，请等待5秒...").arg(f.name));

                        QTimer::singleShot(5000, this, [=]() {
                            Furniture* fPtr = getFurnitureById(id);
                            if (fPtr) {
                                fPtr->fault = 0;
                                updateFurnitureInDb(*fPtr);
                                QMessageBox::information(this, "修复完成", QString("%1已修复，可以正常使用").arg(f.name));

                                if (fPtr->room == "客厅" && fPtr->type == "light") {
                                    if (fPtr->status) {
                                        livingRoomLightStatusLabel->setText("客厅灯: 开启");
                                        livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #f39c12;");
                                        livingRoomLightBrightnessLabel->setText(QString("亮度: %1%").arg(fPtr->brightness));
                                    }
                                    livingRoomLightBtn->setStyleSheet("QPushButton { background-color: #f1c40f; font-size: 14px; border-radius: 5px; }"
                                                                      "QPushButton:hover { background-color: #f39c12; }");
                                }

                                updateDeviceStats();
                                showFurnitureList(category);
                            }
                        });

                        OperationRecord record;
                        record.username = m_currentUser.isEmpty() ? "admin" : m_currentUser;
                        record.deviceName = f.name;
                        record.operation = "修复" + f.name;
                        record.result = "修复中";
                        record.operateTime = QDateTime::currentDateTime();
                        insertOperationRecord(record);
                    }
                });
            }

        } else if (category == "aircon") {
            // ========== 空调控制界面 ==========
            QVBoxLayout *layout = new QVBoxLayout(groupBox);
            layout->setSpacing(8);

            QString statusText;
            QString statusColor;
            if (f.fault == 1) {
                statusText = "状态: 故障";
                statusColor = "#e74c3c";
            } else {
                statusText = f.status ? "状态: 开启" : "状态: 关闭";
                statusColor = f.status ? "#3498db" : "#7f8c8d";
            }

            QLabel *statusLabel = new QLabel(statusText);
            statusLabel->setStyleSheet(QString("font-size: 13px; color: %1;").arg(statusColor));

            QPushButton *toggleBtn = new QPushButton(f.status ? "关闭空调" : "开启空调");
            toggleBtn->setFixedSize(120, 35);

            if (f.fault == 1) {
                toggleBtn->setEnabled(false);
                toggleBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; border-radius: 5px; }");
            } else {
                toggleBtn->setStyleSheet("QPushButton { background-color: #3498db; color: white; font-weight: bold; border-radius: 5px; }"
                                         "QPushButton:hover { background-color: #2980b9; }");
            }

            QPushButton *fixBtn = nullptr;
            if (f.fault == 1) {
                fixBtn = new QPushButton("🔧 修复设备");
                fixBtn->setFixedSize(120, 30);
                fixBtn->setStyleSheet("QPushButton { background-color: #27ae60; color: white; font-weight: bold; border-radius: 5px; }"
                                      "QPushButton:hover { background-color: #229954; }");
                layout->addWidget(fixBtn, 0, Qt::AlignCenter);
            }

            QHBoxLayout *tempLayout = new QHBoxLayout;
            QLabel *tempLabel = new QLabel(QString("温度: %1°C").arg(f.temperature));
            tempLabel->setStyleSheet(f.fault == 1 ? "font-size: 13px; color: #e74c3c;" : "font-size: 13px; color: #3498db; font-weight: bold;");

            QPushButton *tempDownBtn = new QPushButton("-");
            QPushButton *tempUpBtn = new QPushButton("+");
            tempDownBtn->setFixedSize(35, 30);
            tempUpBtn->setFixedSize(35, 30);

            if (f.fault == 1 || !f.status) {
                tempDownBtn->setEnabled(false);
                tempUpBtn->setEnabled(false);
            }

            tempDownBtn->setStyleSheet(f.fault == 1 ? "QPushButton { background-color: #e74c3c; color: white; font-size: 16px; font-weight: bold; border-radius: 5px; }"
                                                    : "QPushButton { background-color: #3498db; color: white; font-size: 16px; font-weight: bold; border-radius: 5px; }");
            tempUpBtn->setStyleSheet(f.fault == 1 ? "QPushButton { background-color: #e74c3c; color: white; font-size: 16px; font-weight: bold; border-radius: 5px; }"
                                                  : "QPushButton { background-color: #3498db; color: white; font-size: 16px; font-weight: bold; border-radius: 5px; }");

            tempLayout->addWidget(tempLabel);
            tempLayout->addWidget(tempDownBtn);
            tempLayout->addWidget(tempUpBtn);

            layout->addWidget(statusLabel, 0, Qt::AlignCenter);
            layout->addWidget(toggleBtn, 0, Qt::AlignCenter);
            layout->addLayout(tempLayout);

            int id = f.id;
            if (f.fault == 0) {
                connect(toggleBtn, &QPushButton::clicked, this, [=]() {
                    controlAirconById(id);
                    QTimer::singleShot(100, this, [=]() {
                        showFurnitureList(category);
                    });
                });
                connect(tempUpBtn, &QPushButton::clicked, this, [=]() {
                    tempUpById(id);
                    QTimer::singleShot(100, this, [=]() {
                        showFurnitureList(category);
                    });
                });
                connect(tempDownBtn, &QPushButton::clicked, this, [=]() {
                    tempDownById(id);
                    QTimer::singleShot(100, this, [=]() {
                        showFurnitureList(category);
                    });
                });
            }

            if (fixBtn) {
                connect(fixBtn, &QPushButton::clicked, this, [=]() {
                    QMessageBox msgBox;
                    msgBox.setWindowTitle("⚠️ 设备故障");
                    msgBox.setText(QString("%1当前处于故障状态，是否立即修复？").arg(f.name));
                    msgBox.setIcon(QMessageBox::Question);

                    QPushButton *fixActionBtn = msgBox.addButton("立即修复（5秒）", QMessageBox::ActionRole);
                    QPushButton *cancelBtn = msgBox.addButton("取消", QMessageBox::RejectRole);

                    msgBox.exec();

                    if (msgBox.clickedButton() == fixActionBtn) {
                        QMessageBox::information(this, "修复中", QString("正在修复%1，请等待5秒...").arg(f.name));

                        QTimer::singleShot(5000, this, [=]() {
                            Furniture* fPtr = getFurnitureById(id);
                            if (fPtr) {
                                fPtr->fault = 0;
                                updateFurnitureInDb(*fPtr);
                                QMessageBox::information(this, "修复完成", QString("%1已修复，可以正常使用").arg(f.name));

                                if (fPtr->room == "客厅" && fPtr->type == "aircon") {
                                    if (fPtr->status) {
                                        livingRoomAirconStatusLabel->setText("客厅空调: 开启");
                                        livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #3498db;");
                                        livingRoomAirconTempLabel->setText(QString("温度: %1°C").arg(fPtr->temperature));
                                    }
                                    livingRoomAirconBtn->setStyleSheet("QPushButton { background-color: #3498db; color: white; font-size: 14px; border-radius: 5px; }"
                                                                       "QPushButton:hover { background-color: #2980b9; }");
                                }

                                updateDeviceStats();
                                showFurnitureList(category);
                            }
                        });

                        OperationRecord record;
                        record.username = m_currentUser.isEmpty() ? "admin" : m_currentUser;
                        record.deviceName = f.name;
                        record.operation = "修复" + f.name;
                        record.result = "修复中";
                        record.operateTime = QDateTime::currentDateTime();
                        insertOperationRecord(record);
                    }
                });
            }

        } else if (category == "camera") {
            // ========== 摄像头控制界面 ==========
            QVBoxLayout *layout = new QVBoxLayout(groupBox);
            layout->setSpacing(8);

            QString statusText;
            QString statusColor;
            if (f.fault == 1) {
                statusText = "状态: 故障";
                statusColor = "#e74c3c";
            } else {
                statusText = f.status ? "状态: 开启" : "状态: 关闭";
                statusColor = f.status ? "#e74c3c" : "#7f8c8d";
            }

            QLabel *statusLabel = new QLabel(statusText);
            statusLabel->setStyleSheet(QString("font-size: 13px; color: %1;").arg(statusColor));

            QPushButton *toggleBtn = new QPushButton(f.status ? "关闭摄像头" : "开启摄像头");
            toggleBtn->setFixedSize(120, 35);

            if (f.fault == 1) {
                toggleBtn->setEnabled(false);
                toggleBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; border-radius: 5px; }");
            } else {
                toggleBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; border-radius: 5px; }"
                                         "QPushButton:hover { background-color: #c0392b; }");
            }

            QPushButton *fixBtn = nullptr;
            if (f.fault == 1) {
                fixBtn = new QPushButton("🔧 修复设备");
                fixBtn->setFixedSize(120, 30);
                fixBtn->setStyleSheet("QPushButton { background-color: #27ae60; color: white; font-weight: bold; border-radius: 5px; }"
                                      "QPushButton:hover { background-color: #229954; }");
                layout->addWidget(fixBtn, 0, Qt::AlignCenter);
            }

            QHBoxLayout *angleLayout = new QHBoxLayout;
            QLabel *angleLabel = new QLabel(QString("角度: %1°").arg(f.angle));
            angleLabel->setStyleSheet(f.fault == 1 ? "font-size: 13px; color: #e74c3c;" : "font-size: 13px; color: #e74c3c; font-weight: bold;");

            QPushButton *leftBtn = new QPushButton("←");
            QPushButton *rightBtn = new QPushButton("→");
            leftBtn->setFixedSize(35, 30);
            rightBtn->setFixedSize(35, 30);

            if (f.fault == 1 || !f.status) {
                leftBtn->setEnabled(false);
                rightBtn->setEnabled(false);
            }

            leftBtn->setStyleSheet(f.fault == 1 ? "QPushButton { background-color: #e74c3c; color: white; font-size: 16px; font-weight: bold; border-radius: 5px; }"
                                                : "QPushButton { background-color: #e74c3c; color: white; font-size: 16px; font-weight: bold; border-radius: 5px; }");
            rightBtn->setStyleSheet(f.fault == 1 ? "QPushButton { background-color: #e74c3c; color: white; font-size: 16px; font-weight: bold; border-radius: 5px; }"
                                                 : "QPushButton { background-color: #e74c3c; color: white; font-size: 16px; font-weight: bold; border-radius: 5px; }");

            QSlider *angleSlider = new QSlider(Qt::Horizontal);
            angleSlider->setRange(-90, 90);
            angleSlider->setValue(f.angle);
            angleSlider->setEnabled(f.status && f.fault == 0);
            angleSlider->setFixedHeight(25);
            angleSlider->setStyleSheet(
                "QSlider::groove:horizontal { height: 4px; background: #bdc3c7; border-radius: 2px; }"
                "QSlider::handle:horizontal { width: 14px; height: 14px; background: #e74c3c; margin: -5px 0; border-radius: 7px; }"
                );

            angleLayout->addWidget(angleLabel);
            angleLayout->addWidget(leftBtn);
            angleLayout->addWidget(rightBtn);

            layout->addWidget(statusLabel, 0, Qt::AlignCenter);
            layout->addWidget(toggleBtn, 0, Qt::AlignCenter);
            layout->addLayout(angleLayout);
            layout->addWidget(angleSlider);

            int id = f.id;
            if (f.fault == 0) {
                connect(toggleBtn, &QPushButton::clicked, this, [=]() {
                    controlCameraById(id);
                    QTimer::singleShot(100, this, [=]() {
                        showFurnitureList(category);
                    });
                });
                connect(leftBtn, &QPushButton::clicked, this, [=]() {
                    cameraLeftById(id);
                    QTimer::singleShot(100, this, [=]() {
                        showFurnitureList(category);
                    });
                });
                connect(rightBtn, &QPushButton::clicked, this, [=]() {
                    cameraRightById(id);
                    QTimer::singleShot(100, this, [=]() {
                        showFurnitureList(category);
                    });
                });
                connect(angleSlider, &QSlider::valueChanged, this, [=](int value) {
                    cameraAngleChangedById(id, value);
                    QTimer::singleShot(100, this, [=]() {
                        showFurnitureList(category);
                    });
                });
            }

            if (fixBtn) {
                connect(fixBtn, &QPushButton::clicked, this, [=]() {
                    QMessageBox msgBox;
                    msgBox.setWindowTitle("⚠️ 设备故障");
                    msgBox.setText(QString("%1当前处于故障状态，是否立即修复？").arg(f.name));
                    msgBox.setIcon(QMessageBox::Question);

                    QPushButton *fixActionBtn = msgBox.addButton("立即修复（5秒）", QMessageBox::ActionRole);
                    QPushButton *cancelBtn = msgBox.addButton("取消", QMessageBox::RejectRole);

                    msgBox.exec();

                    if (msgBox.clickedButton() == fixActionBtn) {
                        QMessageBox::information(this, "修复中", QString("正在修复%1，请等待5秒...").arg(f.name));

                        QTimer::singleShot(5000, this, [=]() {
                            Furniture* fPtr = getFurnitureById(id);
                            if (fPtr) {
                                fPtr->fault = 0;
                                updateFurnitureInDb(*fPtr);
                                QMessageBox::information(this, "修复完成", QString("%1已修复，可以正常使用").arg(f.name));

                                updateDeviceStats();
                                showFurnitureList(category);
                            }
                        });

                        OperationRecord record;
                        record.username = m_currentUser.isEmpty() ? "admin" : m_currentUser;
                        record.deviceName = f.name;
                        record.operation = "修复" + f.name;
                        record.result = "修复中";
                        record.operateTime = QDateTime::currentDateTime();
                        insertOperationRecord(record);
                    }
                });
            }

        } else if (category == "curtain") {
            // ========== 窗帘控制界面 ==========
            QVBoxLayout *layout = new QVBoxLayout(groupBox);
            layout->setSpacing(8);

            QString statusText;
            QString statusColor;
            if (f.fault == 1) {
                statusText = "状态: 故障";
                statusColor = "#e74c3c";
            } else {
                statusText = f.status ? "状态: 开启" : "状态: 关闭";
                statusColor = f.status ? "#9b59b6" : "#7f8c8d";
            }

            QLabel *statusLabel = new QLabel(statusText);
            statusLabel->setStyleSheet(QString("font-size: 13px; color: %1;").arg(statusColor));

            QPushButton *toggleBtn = new QPushButton(f.status ? "关闭窗帘" : "开启窗帘");
            toggleBtn->setFixedSize(120, 35);

            if (f.fault == 1) {
                toggleBtn->setEnabled(false);
                toggleBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; border-radius: 5px; }");
            } else {
                toggleBtn->setStyleSheet("QPushButton { background-color: #9b59b6; color: white; font-weight: bold; border-radius: 5px; }"
                                         "QPushButton:hover { background-color: #8e44ad; }");
            }

            QPushButton *fixBtn = nullptr;
            if (f.fault == 1) {
                fixBtn = new QPushButton("🔧 修复设备");
                fixBtn->setFixedSize(120, 30);
                fixBtn->setStyleSheet("QPushButton { background-color: #27ae60; color: white; font-weight: bold; border-radius: 5px; }"
                                      "QPushButton:hover { background-color: #229954; }");
                layout->addWidget(fixBtn, 0, Qt::AlignCenter);
            }

            QHBoxLayout *opennessLayout = new QHBoxLayout;
            QLabel *opennessLabel = new QLabel(QString("开度: %1%").arg(f.openness));
            opennessLabel->setStyleSheet(f.fault == 1 ? "font-size: 13px; color: #e74c3c;" : "font-size: 13px; color: #9b59b6; font-weight: bold;");

            QPushButton *downBtn = new QPushButton("-");
            QPushButton *upBtn = new QPushButton("+");
            downBtn->setFixedSize(35, 30);
            upBtn->setFixedSize(35, 30);

            if (f.fault == 1 || !f.status) {
                downBtn->setEnabled(false);
                upBtn->setEnabled(false);
            }

            downBtn->setStyleSheet(f.fault == 1 ? "QPushButton { background-color: #e74c3c; color: white; font-size: 16px; font-weight: bold; border-radius: 5px; }"
                                                : "QPushButton { background-color: #9b59b6; color: white; font-size: 16px; font-weight: bold; border-radius: 5px; }");
            upBtn->setStyleSheet(f.fault == 1 ? "QPushButton { background-color: #e74c3c; color: white; font-size: 16px; font-weight: bold; border-radius: 5px; }"
                                              : "QPushButton { background-color: #9b59b6; color: white; font-size: 16px; font-weight: bold; border-radius: 5px; }");

            opennessLayout->addWidget(opennessLabel);
            opennessLayout->addWidget(downBtn);
            opennessLayout->addWidget(upBtn);

            layout->addWidget(statusLabel, 0, Qt::AlignCenter);
            layout->addWidget(toggleBtn, 0, Qt::AlignCenter);
            layout->addLayout(opennessLayout);

            int id = f.id;
            if (f.fault == 0) {
                connect(toggleBtn, &QPushButton::clicked, this, [=]() {
                    controlCurtainById(id);
                    QTimer::singleShot(100, this, [=]() {
                        showFurnitureList(category);
                    });
                });
                connect(upBtn, &QPushButton::clicked, this, [=]() {
                    opennessUpById(id);
                    QTimer::singleShot(100, this, [=]() {
                        showFurnitureList(category);
                    });
                });
                connect(downBtn, &QPushButton::clicked, this, [=]() {
                    opennessDownById(id);
                    QTimer::singleShot(100, this, [=]() {
                        showFurnitureList(category);
                    });
                });
            }

            if (fixBtn) {
                connect(fixBtn, &QPushButton::clicked, this, [=]() {
                    QMessageBox msgBox;
                    msgBox.setWindowTitle("⚠️ 设备故障");
                    msgBox.setText(QString("%1当前处于故障状态，是否立即修复？").arg(f.name));
                    msgBox.setIcon(QMessageBox::Question);

                    QPushButton *fixActionBtn = msgBox.addButton("立即修复（5秒）", QMessageBox::ActionRole);
                    QPushButton *cancelBtn = msgBox.addButton("取消", QMessageBox::RejectRole);

                    msgBox.exec();

                    if (msgBox.clickedButton() == fixActionBtn) {
                        QMessageBox::information(this, "修复中", QString("正在修复%1，请等待5秒...").arg(f.name));

                        QTimer::singleShot(5000, this, [=]() {
                            Furniture* fPtr = getFurnitureById(id);
                            if (fPtr) {
                                fPtr->fault = 0;
                                updateFurnitureInDb(*fPtr);
                                QMessageBox::information(this, "修复完成", QString("%1已修复，可以正常使用").arg(f.name));

                                updateDeviceStats();
                                showFurnitureList(category);
                            }
                        });

                        OperationRecord record;
                        record.username = m_currentUser.isEmpty() ? "admin" : m_currentUser;
                        record.deviceName = f.name;
                        record.operation = "修复" + f.name;
                        record.result = "修复中";
                        record.operateTime = QDateTime::currentDateTime();
                        insertOperationRecord(record);
                    }
                });
            }
        }

        // 将设备卡片添加到网格布局
        gridLayout->addWidget(groupBox, row, col);
        furnitureItemWidgets.append(groupBox);

        col++;
        if (col >= COLS) {
            col = 0;
            row++;
        }
    }

    // 将网格布局添加到主布局
    furnitureListLayout->addLayout(gridLayout);
    furnitureListLayout->addStretch();

    // 切换到设备列表页
    stackedWidget->setCurrentWidget(furnitureListPage);

    // 使用成员变量确保滚动到顶部
    if (furnitureScrollArea) {
        furnitureScrollArea->verticalScrollBar()->setValue(0);
    }
}
// ==================== 返回分类页 ====================
void MainWindow::backToCategory()
{
    stackedWidget->setCurrentWidget(furnitureCategoryPage);
}

// ==================== 家具控制函数 ====================

void MainWindow::controlLightById(int id)
{
    Furniture* f = getFurnitureById(id);
    if (!f) return;

    if (f->fault) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("⚠️ 设备故障");
        msgBox.setText(QString("%1当前处于故障状态，是否立即修复？").arg(f->name));
        msgBox.setIcon(QMessageBox::Question);

        QPushButton *fixBtn = msgBox.addButton("立即修复（5秒）", QMessageBox::ActionRole);
        QPushButton *cancelBtn = msgBox.addButton("取消", QMessageBox::RejectRole);

        msgBox.exec();

        if (msgBox.clickedButton() == fixBtn) {
            QMessageBox::information(this, "修复中", QString("正在修复%1，请等待5秒...").arg(f->name));

            QTimer::singleShot(5000, this, [=]() {
                f->fault = 0;
                updateFurnitureInDb(*f);
                QMessageBox::information(this, "修复完成", QString("%1已修复，可以正常使用").arg(f->name));

                // 更新首页客厅灯状态（如果是客厅灯）
                if (f->room == "客厅") {
                    if (f->status) {
                        livingRoomLightStatusLabel->setText("客厅灯: 开启");
                        livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #f39c12;");
                        livingRoomLightBrightnessLabel->setText(QString("亮度: %1%").arg(f->brightness));
                        // 恢复按钮样式
                        livingRoomLightBtn->setStyleSheet("QPushButton { background-color: #f1c40f; font-size: 14px; border-radius: 5px; }"
                                                          "QPushButton:hover { background-color: #f39c12; }");
                    } else {
                        livingRoomLightStatusLabel->setText("客厅灯: 关闭");
                        livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");
                        livingRoomLightBrightnessLabel->setText("亮度: --");
                        livingRoomLightBtn->setStyleSheet("QPushButton { background-color: #f1c40f; font-size: 14px; border-radius: 5px; }"
                                                          "QPushButton:hover { background-color: #f39c12; }");
                    }
                }
                updateDeviceStats();
            });

            OperationRecord record;
            record.username = m_currentUser.isEmpty() ? "admin" : m_currentUser;
            record.deviceName = f->name;
            record.operation = "修复" + f->name;
            record.result = "修复中";
            record.operateTime = QDateTime::currentDateTime();
            insertOperationRecord(record);
        }
        return;
    }

    f->status = !f->status;
    updateFurnitureInDb(*f);

    // 更新首页客厅灯状态（如果是客厅灯）
    if (f->room == "客厅") {
        if (f->status) {
            livingRoomLightStatusLabel->setText("客厅灯: 开启");
            livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #f39c12;");
            livingRoomLightBrightnessLabel->setText(QString("亮度: %1%").arg(f->brightness));
        } else {
            livingRoomLightStatusLabel->setText("客厅灯: 关闭");
            livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");
            livingRoomLightBrightnessLabel->setText("亮度: --");
        }
    }

    OperationRecord record;
    record.username = m_currentUser.isEmpty() ? "admin" : m_currentUser;
    record.deviceName = f->name;
    record.operation = f->status ? "开启" : "关闭";
    record.result = "成功";
    record.operateTime = QDateTime::currentDateTime();
    insertOperationRecord(record);

    QMessageBox::information(this, "提示", QString("%1已%2").arg(f->name).arg(f->status ? "开启" : "关闭"));
}

void MainWindow::controlAirconById(int id)
{
    Furniture* f = getFurnitureById(id);
    if (!f) return;

    if (f->fault) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("⚠️ 设备故障");
        msgBox.setText(QString("%1当前处于故障状态，是否立即修复？").arg(f->name));
        msgBox.setIcon(QMessageBox::Question);

        QPushButton *fixBtn = msgBox.addButton("立即修复（5秒）", QMessageBox::ActionRole);
        QPushButton *cancelBtn = msgBox.addButton("取消", QMessageBox::RejectRole);

        msgBox.exec();

        if (msgBox.clickedButton() == fixBtn) {
            QMessageBox::information(this, "修复中", QString("正在修复%1，请等待5秒...").arg(f->name));

            QTimer::singleShot(5000, this, [=]() {
                f->fault = 0;
                updateFurnitureInDb(*f);
                QMessageBox::information(this, "修复完成", QString("%1已修复，可以正常使用").arg(f->name));

                // 更新首页客厅空调状态（如果是客厅空调）
                if (f->room == "客厅") {
                    if (f->status) {
                        livingRoomAirconStatusLabel->setText("客厅空调: 开启");
                        livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #3498db;");
                        livingRoomAirconTempLabel->setText(QString("温度: %1°C").arg(f->temperature));
                        // 恢复按钮样式
                        livingRoomAirconBtn->setStyleSheet("QPushButton { background-color: #3498db; color: white; font-size: 14px; border-radius: 5px; }"
                                                           "QPushButton:hover { background-color: #2980b9; }");
                    } else {
                        livingRoomAirconStatusLabel->setText("客厅空调: 关闭");
                        livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");
                        livingRoomAirconTempLabel->setText("温度: --");
                        livingRoomAirconBtn->setStyleSheet("QPushButton { background-color: #3498db; color: white; font-size: 14px; border-radius: 5px; }"
                                                           "QPushButton:hover { background-color: #2980b9; }");
                    }
                }
                updateDeviceStats();
            });

            OperationRecord record;
            record.username = m_currentUser.isEmpty() ? "admin" : m_currentUser;
            record.deviceName = f->name;
            record.operation = "修复" + f->name;
            record.result = "修复中";
            record.operateTime = QDateTime::currentDateTime();
            insertOperationRecord(record);
        }
        return;
    }

    f->status = !f->status;
    updateFurnitureInDb(*f);

    // 更新首页客厅空调状态（如果是客厅空调）
    if (f->room == "客厅") {
        if (f->status) {
            livingRoomAirconStatusLabel->setText("客厅空调: 开启");
            livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #3498db;");
            livingRoomAirconTempLabel->setText(QString("温度: %1°C").arg(f->temperature));
        } else {
            livingRoomAirconStatusLabel->setText("客厅空调: 关闭");
            livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");
            livingRoomAirconTempLabel->setText("温度: --");
        }
    }

    OperationRecord record;
    record.username = m_currentUser.isEmpty() ? "admin" : m_currentUser;
    record.deviceName = f->name;
    record.operation = f->status ? "开启空调" : "关闭空调";
    record.result = "成功";
    record.operateTime = QDateTime::currentDateTime();
    insertOperationRecord(record);

    QMessageBox::information(this, "提示", QString("%1已%2").arg(f->name).arg(f->status ? "开启" : "关闭"));
}

void MainWindow::controlCurtainById(int id)
{
    Furniture* f = getFurnitureById(id);
    if (!f) return;

    f->status = !f->status;
    updateFurnitureInDb(*f);
    showFurnitureList(m_currentCategory);

    OperationRecord record;
    record.username = m_currentUser.isEmpty() ? "admin" : m_currentUser;
    record.deviceName = f->name;
    record.operation = f->status ? "开启窗帘" : "关闭窗帘";
    record.result = "成功";
    record.operateTime = QDateTime::currentDateTime();
    insertOperationRecord(record);

    QMessageBox::information(this, "提示", QString("%1已%2").arg(f->name).arg(f->status ? "开启" : "关闭"));
}

void MainWindow::controlCameraById(int id)
{
    Furniture* f = getFurnitureById(id);
    if (!f) return;

    f->status = !f->status;
    updateFurnitureInDb(*f);
    showFurnitureList(m_currentCategory);

    OperationRecord record;
    record.username = m_currentUser.isEmpty() ? "admin" : m_currentUser;
    record.deviceName = f->name;
    record.operation = f->status ? "开启摄像头" : "关闭摄像头";
    record.result = "成功";
    record.operateTime = QDateTime::currentDateTime();
    insertOperationRecord(record);

    QMessageBox::information(this, "提示", QString("%1已%2").arg(f->name).arg(f->status ? "开启" : "关闭"));
}

void MainWindow::brightnessUpById(int id)
{
    Furniture* f = getFurnitureById(id);
    if (!f || !f->status) return;

    if (f->brightness < 100) {
        f->brightness = qMin(100, f->brightness + 10);
        updateFurnitureInDb(*f);

        // 更新首页客厅灯亮度（如果是客厅灯）
        if (f->room == "客厅") {
            livingRoomLightBrightnessLabel->setText(QString("亮度: %1%").arg(f->brightness));
        }

        showFurnitureList(m_currentCategory);
    }
}

void MainWindow::brightnessDownById(int id)
{
    Furniture* f = getFurnitureById(id);
    if (!f || !f->status) return;

    if (f->brightness > 0) {
        f->brightness = qMax(0, f->brightness - 10);
        updateFurnitureInDb(*f);

        if (f->room == "客厅") {
            livingRoomLightBrightnessLabel->setText(QString("亮度: %1%").arg(f->brightness));
        }

        showFurnitureList(m_currentCategory);
    }
}

void MainWindow::tempUpById(int id)
{
    Furniture* f = getFurnitureById(id);
    if (!f || !f->status) return;

    if (f->temperature < 30) {
        f->temperature++;
        updateFurnitureInDb(*f);

        if (f->room == "客厅") {
            livingRoomAirconTempLabel->setText(QString("温度: %1°C").arg(f->temperature));
        }

        showFurnitureList(m_currentCategory);
    }
}

void MainWindow::tempDownById(int id)
{
    Furniture* f = getFurnitureById(id);
    if (!f || !f->status) return;

    if (f->temperature > 16) {
        f->temperature--;
        updateFurnitureInDb(*f);

        if (f->room == "客厅") {
            livingRoomAirconTempLabel->setText(QString("温度: %1°C").arg(f->temperature));
        }

        showFurnitureList(m_currentCategory);
    }
}

void MainWindow::opennessUpById(int id)
{
    Furniture* f = getFurnitureById(id);
    if (!f || !f->status) return;

    if (f->openness < 100) {
        f->openness = qMin(100, f->openness + 10);
        updateFurnitureInDb(*f);
        showFurnitureList(m_currentCategory);
    }
}

void MainWindow::opennessDownById(int id)
{
    Furniture* f = getFurnitureById(id);
    if (!f || !f->status) return;

    if (f->openness > 0) {
        f->openness = qMax(0, f->openness - 10);
        updateFurnitureInDb(*f);
        showFurnitureList(m_currentCategory);
    }
}

void MainWindow::cameraLeftById(int id)
{
    Furniture* f = getFurnitureById(id);
    if (!f || !f->status) return;

    if (f->angle > -90) {
        f->angle = qMax(-90, f->angle - 15);
        updateFurnitureInDb(*f);
        showFurnitureList(m_currentCategory);
    }
}

void MainWindow::cameraRightById(int id)
{
    Furniture* f = getFurnitureById(id);
    if (!f || !f->status) return;

    if (f->angle < 90) {
        f->angle = qMin(90, f->angle + 15);
        updateFurnitureInDb(*f);
        showFurnitureList(m_currentCategory);
    }
}

void MainWindow::cameraAngleChangedById(int id, int angle)
{
    Furniture* f = getFurnitureById(id);
    if (!f || !f->status) return;

    f->angle = angle;
    updateFurnitureInDb(*f);
    showFurnitureList(m_currentCategory);
}

// ==================== 首页客厅设备控制 ====================

void MainWindow::addAlarmRecord(QString type, QString content, QString alarmType)
{
    QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString record = QString("[%1] %2: %3").arg(time).arg(type).arg(content);
    alarmList->insertItem(0, record);

    // 设置颜色
    if (type.contains("温度")) {
        alarmList->item(0)->setForeground(QColor(230, 126, 34));
    } else if (type.contains("湿度")) {
        alarmList->item(0)->setForeground(QColor(52, 152, 219));
    } else if (type.contains("光照")) {
        alarmList->item(0)->setForeground(QColor(241, 196, 15));
    } else {
        alarmList->item(0)->setForeground(QColor(192, 57, 43));
    }

    // 弹出提示框（带一键处理）
    QMessageBox msgBox;
    msgBox.setWindowTitle("⚠️ 报警");
    msgBox.setText(type + ": " + content);
    msgBox.setIcon(QMessageBox::Warning);

    QPushButton *handleBtn = nullptr;
    if (alarmType == "light_high") {
        handleBtn = msgBox.addButton("降下窗帘", QMessageBox::ActionRole);
    } else if (alarmType == "light_low") {
        handleBtn = msgBox.addButton("打开灯光", QMessageBox::ActionRole);
    } else if (alarmType == "temperature_high") {
        handleBtn = msgBox.addButton("开启空调", QMessageBox::ActionRole);
    } else if (alarmType == "temperature_low") {
        handleBtn = msgBox.addButton("关闭空调", QMessageBox::ActionRole);
    } else if (alarmType == "humidity_high") {
        handleBtn = msgBox.addButton("开启除湿", QMessageBox::ActionRole);
    } else if (alarmType == "humidity_low") {
        handleBtn = msgBox.addButton("开启加湿", QMessageBox::ActionRole);
    }

    QPushButton *ignoreBtn = msgBox.addButton("不再提示此类型", QMessageBox::ActionRole);
    QPushButton *okBtn = msgBox.addButton("确定", QMessageBox::AcceptRole);

    msgBox.exec();

    if (handleBtn && msgBox.clickedButton() == handleBtn) {
        if (alarmType == "light_high") {
            int newLight = lightMax - 50;
            lightSlider->setValue(newLight);
            QMessageBox::information(this, "处理", QString("窗帘已降下，光照降至%1 lux").arg(newLight));
        } else if (alarmType == "light_low") {
            int newLight = lightMin + 50;
            lightSlider->setValue(newLight);
            QMessageBox::information(this, "处理", QString("灯光已开启，光照升至%1 lux").arg(newLight));

            // 打开客厅灯
            Furniture* light = getLivingRoomLight();
            if (light) {
                light->status = 1;
                light->brightness = 80;
                updateFurnitureInDb(*light);

                livingRoomLightStatusLabel->setText("客厅灯: 开启");
                livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #f39c12;");
                livingRoomLightBrightnessLabel->setText(QString("亮度: %1%").arg(light->brightness));
            }
        } else if (alarmType == "temperature_high") {
            int newTemp = tempMax - 2;
            tempSlider->setValue(newTemp);
            QMessageBox::information(this, "处理", QString("空调已开启，温度降至%1°C").arg(newTemp));

            // 打开客厅空调
            Furniture* aircon = getLivingRoomAircon();
            if (aircon) {
                aircon->status = 1;
                aircon->temperature = 24;
                updateFurnitureInDb(*aircon);

                livingRoomAirconStatusLabel->setText("客厅空调: 开启");
                livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #3498db;");
                livingRoomAirconTempLabel->setText("温度: 24°C");
            }
        } else if (alarmType == "temperature_low") {
            int newTemp = tempMin + 2;
            tempSlider->setValue(newTemp);
            QMessageBox::information(this, "处理", QString("空调已关闭，温度升至%1°C").arg(newTemp));

            // 关闭客厅空调
            Furniture* aircon = getLivingRoomAircon();
            if (aircon) {
                aircon->status = 0;
                updateFurnitureInDb(*aircon);

                livingRoomAirconStatusLabel->setText("客厅空调: 关闭");
                livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");
                livingRoomAirconTempLabel->setText("温度: --");
            }
        } else if (alarmType == "humidity_high") {
            int newHumi = humiMax - 10;
            humiSlider->setValue(newHumi);
            QMessageBox::information(this, "处理", QString("除湿已开启，湿度降至%1%").arg(newHumi));
        } else if (alarmType == "humidity_low") {
            int newHumi = humiMin + 10;
            humiSlider->setValue(newHumi);
            QMessageBox::information(this, "处理", QString("加湿已开启，湿度升至%1%").arg(newHumi));
        }
    }

    if (msgBox.clickedButton() == ignoreBtn) {
        ignoredAlarms.insert(type);
    }
}
void MainWindow::clearAlarm()
{
    alarmList->clear();
    ignoredAlarms.clear();

    // 重置所有故障状态
    for (int i = 0; i < m_furniture.size(); ++i) {
        if (m_furniture[i].fault == 1) {
            m_furniture[i].fault = 0;
            updateFurnitureInDb(m_furniture[i]);

            // 更新首页状态（如果是客厅设备）
            if (m_furniture[i].room == "客厅") {
                if (m_furniture[i].type == "light") {
                    if (m_furniture[i].status) {
                        livingRoomLightStatusLabel->setText("客厅灯: 开启");
                        livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #f39c12;");
                        livingRoomLightBrightnessLabel->setText(QString("亮度: %1%").arg(m_furniture[i].brightness));
                    } else {
                        livingRoomLightStatusLabel->setText("客厅灯: 关闭");
                        livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");
                        livingRoomLightBrightnessLabel->setText("亮度: --");
                    }
                    livingRoomLightBtn->setStyleSheet("QPushButton { background-color: #f1c40f; font-size: 14px; border-radius: 5px; }");
                } else if (m_furniture[i].type == "aircon") {
                    if (m_furniture[i].status) {
                        livingRoomAirconStatusLabel->setText("客厅空调: 开启");
                        livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #3498db;");
                        livingRoomAirconTempLabel->setText(QString("温度: %1°C").arg(m_furniture[i].temperature));
                    } else {
                        livingRoomAirconStatusLabel->setText("客厅空调: 关闭");
                        livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");
                        livingRoomAirconTempLabel->setText("温度: --");
                    }
                    livingRoomAirconBtn->setStyleSheet("QPushButton { background-color: #3498db; color: white; font-size: 14px; border-radius: 5px; }");
                }
            }
        }
    }

    // 停止修复定时器
    if (lightFixTimer && lightFixTimer->isActive()) lightFixTimer->stop();
    if (airconFixTimer && airconFixTimer->isActive()) airconFixTimer->stop();

    lightFault = false;
    airconFault = false;

    updateDeviceStats();

    QMessageBox::information(this, "提示", "所有报警记录已清除，设备故障已重置");
}
void MainWindow::updateEnvData()
{
    // 这个方法用于更新环境数据
    // 目前由 m_envTimer 定时器处理
}

// ==================== 设备统计相关函数 ====================
void MainWindow::updateDeviceStats()
{
    int oldOnline = m_onlineDevices;
    int oldOffline = m_offlineDevices;
    int oldError = m_errorDevices;

    m_onlineDevices = 0;
    m_offlineDevices = 0;
    m_errorDevices = 0;

    // 统计所有家具的状态
    foreach (const Furniture& f, m_furniture) {
        if (f.fault == 1) {
            m_errorDevices++;
        } else if (f.status == 1) {
            m_onlineDevices++;
        } else {
            m_offlineDevices++;
        }
    }

    // 更新标签文本
    m_onlineLabel->setText(QString("在线设备: %1").arg(m_onlineDevices));
    m_offlineLabel->setText(QString("离线设备: %1").arg(m_offlineDevices));
    m_errorLabel->setText(QString("异常设备: %1").arg(m_errorDevices));

    // 更新标签样式
    QString onlineStyle = m_onlineDevices > 0 ?
                              "QLabel { color: #27ae60; font-size: 14px; font-weight: bold; padding: 8px; border: 2px solid #27ae60; border-radius: 5px; background-color: #e8f8f5; }"
                              "QLabel:hover { background-color: #d5f0e8; border-width: 3px; }" :
                              "QLabel { color: #7f8c8d; font-size: 14px; padding: 8px; border: 1px solid #bdc3c7; border-radius: 5px; background-color: #f8f9f9; }"
                              "QLabel:hover { background-color: #ecf0f1; }";
    m_onlineLabel->setStyleSheet(onlineStyle);

    QString offlineStyle = m_offlineDevices > 0 ?
                               "QLabel { color: #e67e22; font-size: 14px; font-weight: bold; padding: 8px; border: 2px solid #e67e22; border-radius: 5px; background-color: #fef5e7; }"
                               "QLabel:hover { background-color: #fdebd0; border-width: 3px; }" :
                               "QLabel { color: #7f8c8d; font-size: 14px; padding: 8px; border: 1px solid #bdc3c7; border-radius: 5px; background-color: #f8f9f9; }"
                               "QLabel:hover { background-color: #ecf0f1; }";
    m_offlineLabel->setStyleSheet(offlineStyle);

    QString errorStyle = m_errorDevices > 0 ?
                             "QLabel { color: #e74c3c; font-size: 14px; font-weight: bold; padding: 8px; border: 2px solid #e74c3c; border-radius: 5px; background-color: #fdedec; }"
                             "QLabel:hover { background-color: #fadbd8; border-width: 3px; }" :
                             "QLabel { color: #7f8c8d; font-size: 14px; padding: 8px; border: 1px solid #bdc3c7; border-radius: 5px; background-color: #f8f9f9; }"
                             "QLabel:hover { background-color: #ecf0f1; }";
    m_errorLabel->setStyleSheet(errorStyle);

    if (oldOnline != m_onlineDevices || oldOffline != m_offlineDevices || oldError != m_errorDevices) {
        highlightChangedLabels();
    }
}

void MainWindow::highlightChangedLabels()
{
    QString originalOnlineStyle = m_onlineLabel->styleSheet();
    QString originalOfflineStyle = m_offlineLabel->styleSheet();

    m_onlineLabel->setStyleSheet(originalOnlineStyle + "background-color: #f1c40f;");
    m_offlineLabel->setStyleSheet(originalOfflineStyle + "background-color: #f1c40f;");

    QTimer::singleShot(500, this, [=]() {
        m_onlineLabel->setStyleSheet(originalOnlineStyle);
        m_offlineLabel->setStyleSheet(originalOfflineStyle);
    });
}

// ==================== 显示设备列表 ====================
void MainWindow::showOnlineDevices()
{
    QStringList onlineList;

    foreach (const Furniture& f, m_furniture) {
        if (f.status == 1 && f.fault == 0) {
            if (f.type == "light") {
                onlineList << "💡 " + f.name + " (亮度:" + QString::number(f.brightness) + "%)";
            } else if (f.type == "aircon") {
                onlineList << "❄️ " + f.name + " (温度:" + QString::number(f.temperature) + "°C)";
            } else if (f.type == "curtain") {
                onlineList << "🪟 " + f.name + " (开度:" + QString::number(f.openness) + "%)";
            } else if (f.type == "camera") {
                onlineList << "📹 " + f.name + " (角度:" + QString::number(f.angle) + "°)";
            }
        }
    }

    QString message = onlineList.isEmpty() ? "📱 当前没有在线设备" :
                          "📱 <b>在线设备列表</b><br/><br/>" + [&]() {
                              QStringList formatted;
                              for (const QString &d : onlineList) formatted << "  • " + d;
                              return formatted.join("<br/>");
                          }() + QString("<br/><br/><b>总计：%1 个设备在线</b>").arg(onlineList.size());

    QMessageBox::information(this, "在线设备", message);
}

void MainWindow::showOfflineDevices()
{
    QStringList offlineList;

    foreach (const Furniture& f, m_furniture) {
        if (f.status == 0 && f.fault == 0) {
            offlineList << "  • " + f.name;
        }
    }

    QString message = offlineList.isEmpty() ? "📴 当前没有离线设备" :
                          "📴 <b>离线设备列表</b><br/><br/>" + offlineList.join("<br/>") +
                              QString("<br/><br/><b>总计：%1 个设备离线</b>").arg(offlineList.size());

    QMessageBox::information(this, "离线设备", message);
}

void MainWindow::showErrorDevices()
{
    QStringList errorList;

    foreach (const Furniture& f, m_furniture) {
        if (f.fault == 1) {
            QString status = f.status == 1 ? "开启" : "关闭";
            errorList << "  • " + f.name + " (设备故障 - 当前状态:" + status + ")";
        } else if (f.type == "aircon" && f.status == 1) {
            if (f.temperature < 16 || f.temperature > 30) {
                errorList << "  • " + f.name + " (温度异常: " + QString::number(f.temperature) + "°C)";
            }
        } else if (f.type == "camera" && f.status == 1) {
            if (f.angle < -90 || f.angle > 90) {
                errorList << "  • " + f.name + " (角度异常: " + QString::number(f.angle) + "°)";
            }
        }
    }

    QString message;
    if (errorList.isEmpty()) {
        message = "✅ <b>当前没有异常设备</b><br/><br/>所有设备运行正常 ✓";
    } else {
        message = "⚠️ <b>异常设备列表</b><br/><br/>" + errorList.join("<br/>") +
                  QString("<br/><br/><b>总计：%1 个设备异常</b>").arg(errorList.size());
    }

    QMessageBox::information(this, "异常设备", message);
}

// ==================== 事件过滤器 ====================
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            if (obj == m_onlineLabel) {
                showOnlineDevices();
                return true;
            } else if (obj == m_offlineLabel) {
                showOfflineDevices();
                return true;
            } else if (obj == m_errorLabel) {
                showErrorDevices();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}
// ==================== 创建报警监控页 ====================
void MainWindow::createAlarmPage()
{
    alarmPage = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(alarmPage);
    mainLayout->setSpacing(15);  // 减小间距
    mainLayout->setContentsMargins(20, 15, 20, 15);  // 减小边距

    QLabel *title = new QLabel("🚨 报警监控中心");
    title->setStyleSheet("font-size: 22px; font-weight: bold; color: #c0392b;");  // 减小标题字体
    title->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title);

    // ========== 环境数据模拟（水平布局，减少空间占用）==========
    QGroupBox *envGroup = new QGroupBox("🌡️ 环境数据");
    envGroup->setStyleSheet("QGroupBox { font-size: 16px; font-weight: bold; margin-top: 8px; }");
    QGridLayout *envLayout = new QGridLayout(envGroup);  // 改用网格布局
    envLayout->setSpacing(10);
    envLayout->setContentsMargins(10, 15, 10, 15);

    // 第一行：温度和湿度
    QLabel *tempLabel = new QLabel("温度:");
    tempLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    envLayout->addWidget(tempLabel, 0, 0);

    tempValue = new QLabel("25°C");
    tempValue->setStyleSheet("font-size: 18px; font-weight: bold; color: #e67e22; min-width: 60px;");
    envLayout->addWidget(tempValue, 0, 1);

    tempSlider = new QSlider(Qt::Horizontal);
    tempSlider->setRange(0, 50);
    tempSlider->setValue(25);
    tempSlider->setFixedHeight(30);
    tempSlider->setStyleSheet("QSlider::groove:horizontal { height: 8px; background: #bdc3c7; border-radius: 4px; }"
                              "QSlider::handle:horizontal { width: 20px; height: 20px; background: #e67e22; margin: -6px 0; border-radius: 10px; }");
    envLayout->addWidget(tempSlider, 0, 2, 1, 2);

    QLabel *humiLabel = new QLabel("湿度:");
    humiLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    envLayout->addWidget(humiLabel, 1, 0);

    humiValue = new QLabel("50%");
    humiValue->setStyleSheet("font-size: 18px; font-weight: bold; color: #3498db; min-width: 60px;");
    envLayout->addWidget(humiValue, 1, 1);

    humiSlider = new QSlider(Qt::Horizontal);
    humiSlider->setRange(0, 100);
    humiSlider->setValue(50);
    humiSlider->setFixedHeight(30);
    humiSlider->setStyleSheet("QSlider::groove:horizontal { height: 8px; background: #bdc3c7; border-radius: 4px; }"
                              "QSlider::handle:horizontal { width: 20px; height: 20px; background: #3498db; margin: -6px 0; border-radius: 10px; }");
    envLayout->addWidget(humiSlider, 1, 2, 1, 2);

    // 第二行：光照
    QLabel *lightEnvLabel = new QLabel("光照:");
    lightEnvLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    envLayout->addWidget(lightEnvLabel, 2, 0);

    lightValue = new QLabel("500 lux");
    lightValue->setStyleSheet("font-size: 18px; font-weight: bold; color: #f1c40f; min-width: 60px;");
    envLayout->addWidget(lightValue, 2, 1);

    lightSlider = new QSlider(Qt::Horizontal);
    lightSlider->setRange(0, 1000);
    lightSlider->setValue(500);
    lightSlider->setFixedHeight(30);
    lightSlider->setStyleSheet("QSlider::groove:horizontal { height: 8px; background: #bdc3c7; border-radius: 4px; }"
                               "QSlider::handle:horizontal { width: 20px; height: 20px; background: #f1c40f; margin: -6px 0; border-radius: 10px; }");
    envLayout->addWidget(lightSlider, 2, 2, 1, 2);

    // 设置列宽比例
    envLayout->setColumnStretch(0, 0);
    envLayout->setColumnStretch(1, 0);
    envLayout->setColumnStretch(2, 4);
    envLayout->setColumnStretch(3, 1);

    mainLayout->addWidget(envGroup);

    // ========== 报警阈值设置（紧凑布局）==========
    QGroupBox *thresholdGroup = new QGroupBox("⚙️ 阈值设置");
    thresholdGroup->setStyleSheet("QGroupBox { font-size: 16px; font-weight: bold; margin-top: 8px; }");
    QGridLayout *thresholdLayout = new QGridLayout(thresholdGroup);
    thresholdLayout->setSpacing(8);
    thresholdLayout->setContentsMargins(10, 15, 10, 15);

    // 温度阈值
    thresholdLayout->addWidget(new QLabel("温度:"), 0, 0, Qt::AlignRight);

    QHBoxLayout *tempMinMaxLayout = new QHBoxLayout;
    tempMinMaxLayout->setSpacing(5);

    QSpinBox *tempMinSpin = new QSpinBox;
    tempMinSpin->setRange(0, 50);
    tempMinSpin->setValue(18);
    tempMinSpin->setSuffix("°C");
    tempMinSpin->setFixedWidth(80);
    tempMinSpin->setAlignment(Qt::AlignCenter);
    tempMinMaxLayout->addWidget(new QLabel("下限:"));
    tempMinMaxLayout->addWidget(tempMinSpin);
    tempMinMaxLayout->addSpacing(10);

    QSpinBox *tempMaxSpin = new QSpinBox;
    tempMaxSpin->setRange(0, 50);
    tempMaxSpin->setValue(30);
    tempMaxSpin->setSuffix("°C");
    tempMaxSpin->setFixedWidth(80);
    tempMaxSpin->setAlignment(Qt::AlignCenter);
    tempMinMaxLayout->addWidget(new QLabel("上限:"));
    tempMinMaxLayout->addWidget(tempMaxSpin);
    tempMinMaxLayout->addStretch();

    thresholdLayout->addLayout(tempMinMaxLayout, 0, 1);

    // 湿度阈值
    thresholdLayout->addWidget(new QLabel("湿度:"), 1, 0, Qt::AlignRight);

    QHBoxLayout *humiMinMaxLayout = new QHBoxLayout;
    humiMinMaxLayout->setSpacing(5);

    QSpinBox *humiMinSpin = new QSpinBox;
    humiMinSpin->setRange(0, 100);
    humiMinSpin->setValue(30);
    humiMinSpin->setSuffix("%");
    humiMinSpin->setFixedWidth(80);
    humiMinSpin->setAlignment(Qt::AlignCenter);
    humiMinMaxLayout->addWidget(new QLabel("下限:"));
    humiMinMaxLayout->addWidget(humiMinSpin);
    humiMinMaxLayout->addSpacing(10);

    QSpinBox *humiMaxSpin = new QSpinBox;
    humiMaxSpin->setRange(0, 100);
    humiMaxSpin->setValue(70);
    humiMaxSpin->setSuffix("%");
    humiMaxSpin->setFixedWidth(80);
    humiMaxSpin->setAlignment(Qt::AlignCenter);
    humiMinMaxLayout->addWidget(new QLabel("上限:"));
    humiMinMaxLayout->addWidget(humiMaxSpin);
    humiMinMaxLayout->addStretch();

    thresholdLayout->addLayout(humiMinMaxLayout, 1, 1);

    // 光照阈值
    thresholdLayout->addWidget(new QLabel("光照:"), 2, 0, Qt::AlignRight);

    QHBoxLayout *lightMinMaxLayout = new QHBoxLayout;
    lightMinMaxLayout->setSpacing(5);

    QSpinBox *lightMinSpin = new QSpinBox;
    lightMinSpin->setRange(0, 1000);
    lightMinSpin->setValue(200);
    lightMinSpin->setSuffix(" lux");
    lightMinSpin->setFixedWidth(100);
    lightMinSpin->setAlignment(Qt::AlignCenter);
    lightMinMaxLayout->addWidget(new QLabel("下限:"));
    lightMinMaxLayout->addWidget(lightMinSpin);
    lightMinMaxLayout->addSpacing(10);

    QSpinBox *lightMaxSpin = new QSpinBox;
    lightMaxSpin->setRange(0, 1000);
    lightMaxSpin->setValue(800);
    lightMaxSpin->setSuffix(" lux");
    lightMaxSpin->setFixedWidth(100);
    lightMaxSpin->setAlignment(Qt::AlignCenter);
    lightMinMaxLayout->addWidget(new QLabel("上限:"));
    lightMinMaxLayout->addWidget(lightMaxSpin);
    lightMinMaxLayout->addStretch();

    thresholdLayout->addLayout(lightMinMaxLayout, 2, 1);

    mainLayout->addWidget(thresholdGroup);

    // ========== 报警记录 ==========
    QGroupBox *recordGroup = new QGroupBox("📋 报警记录");
    recordGroup->setStyleSheet("QGroupBox { font-size: 16px; font-weight: bold; margin-top: 8px; }");
    QVBoxLayout *recordLayout = new QVBoxLayout(recordGroup);
    recordLayout->setSpacing(8);
    recordLayout->setContentsMargins(10, 15, 10, 10);

    alarmList = new QListWidget;
    alarmList->setMinimumHeight(180);  // 适当减小高度
    alarmList->setMaximumHeight(250);  // 设置最大高度
    alarmList->setStyleSheet("QListWidget { border: 1px solid #bdc3c7; border-radius: 5px; padding: 5px; font-size: 13px; }"
                             "QListWidget::item { padding: 4px; }");
    recordLayout->addWidget(alarmList);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    clearAlarmBtn = new QPushButton("清除记录");
    clearAlarmBtn->setFixedSize(120, 35);  // 减小按钮尺寸
    clearAlarmBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-size: 14px; border-radius: 5px; }"
                                 "QPushButton:hover { background-color: #c0392b; }");
    btnLayout->addWidget(clearAlarmBtn);
    btnLayout->addStretch();
    recordLayout->addLayout(btnLayout);

    mainLayout->addWidget(recordGroup);

    // ========== 返回按钮 ==========
    alarmBackBtn = new QPushButton("🏠 返回首页");
    alarmBackBtn->setFixedSize(180, 40);  // 减小按钮尺寸
    alarmBackBtn->setStyleSheet("QPushButton { background-color: #95a5a6; color: white; font-size: 16px; border-radius: 6px; }"
                                "QPushButton:hover { background-color: #7f8c8d; }");
    mainLayout->addWidget(alarmBackBtn, 0, Qt::AlignCenter);

    // ========== 连接信号 ==========
    connect(tempSlider, &QSlider::valueChanged, this, [=](int value) {
        tempValue->setText(QString("%1°C").arg(value));
    });

    connect(humiSlider, &QSlider::valueChanged, this, [=](int value) {
        humiValue->setText(QString("%1%").arg(value));
    });

    connect(lightSlider, &QSlider::valueChanged, this, [=](int value) {
        lightValue->setText(QString("%1 lux").arg(value));
    });

    connect(tempMinSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int value) {
        tempMin = value;
    });

    connect(tempMaxSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int value) {
        tempMax = value;
    });

    connect(humiMinSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int value) {
        humiMin = value;
    });

    connect(humiMaxSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int value) {
        humiMax = value;
    });

    connect(lightMinSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int value) {
        lightMin = value;
    });

    connect(lightMaxSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int value) {
        lightMax = value;
    });

    connect(clearAlarmBtn, &QPushButton::clicked, this, &MainWindow::clearAlarm);
    connect(alarmBackBtn, &QPushButton::clicked, this, &MainWindow::backToHome);

    stackedWidget->addWidget(alarmPage);
}
// ==================== 创建系统管理页 ====================
void MainWindow::createSysManagePage()
{
    sysManagePage = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(sysManagePage);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(30, 20, 30, 20);

    // 标题
    QLabel *title = new QLabel("⚙️ 系统与设备管理");
    title->setStyleSheet("font-size: 20px; font-weight: bold; color: #2c3e50;");
    title->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title);

    // ===== 1. 设备管理区域 =====
    QGroupBox *devGroup = new QGroupBox("📱 设备管理");
    devGroup->setStyleSheet("QGroupBox { font-size: 16px; font-weight: bold; }");
    QGridLayout *devLayout = new QGridLayout(devGroup);
    devLayout->setSpacing(15);

    // 设备添加
    devLayout->addWidget(new QLabel("设备名称:"), 0, 0);
    devAddName = new QLineEdit;
    devAddName->setPlaceholderText("如：卧室灯光");
    devLayout->addWidget(devAddName, 0, 1);

    // 2. 设备类型（改为下拉框，变量名：devAddType）
    devLayout->addWidget(new QLabel("设备类型:"), 1, 0);
    devAddType = new QComboBox;  // 关键：定义为下拉框，不是重复用devAddGroup

    // 核心修改：addItem(显示文本, 实际存储值)
    devAddType->addItem("灯光", "light");
    devAddType->addItem("空调", "aircon");
    devAddType->addItem("摄像头", "camera");
    devAddType->addItem("窗帘", "curtain");
    devLayout->addWidget(devAddType, 1, 1);


    devLayout->addWidget(new QLabel("设备分组:"), 2, 0);
    devAddGroup = new QComboBox;
    devAddGroup->addItems({"客厅", "卧室", "厨房", "卫生间"});
    devLayout->addWidget(devAddGroup, 2, 1);

    devAddBtn = new QPushButton("添加设备");
    devAddBtn->setStyleSheet("background-color: #2ecc71; color: white; border-radius: 5px;");
    devLayout->addWidget(devAddBtn, 0, 2);

    devDelBtn = new QPushButton("删除选中");
    devDelBtn->setStyleSheet("background-color: #e74c3c; color: white; border-radius: 5px;");
    devLayout->addWidget(devDelBtn, 1, 2);

    devTestBtn = new QPushButton("测试连接");
    devTestBtn->setStyleSheet("background-color: #3498db; color: white; border-radius: 5px;");
    devLayout->addWidget(devTestBtn, 2, 2);

    // 查询行
    QWidget *searchWidget = new QWidget;
    QHBoxLayout *searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setSpacing(10);

    QLineEdit *searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText("输入设备名称/类型/分组查询");
    searchLayout->addWidget(searchEdit);

    QPushButton *searchBtn = new QPushButton("查询");
    searchBtn->setStyleSheet("background-color: #3498db; color: white; border-radius: 5px;");
    searchLayout->addWidget(searchBtn);

    QPushButton *refreshBtn = new QPushButton("刷新全部");
    refreshBtn->setStyleSheet("background-color: #95a5a6; color: white; border-radius: 5px;");
    searchLayout->addWidget(refreshBtn);

    devLayout->addWidget(searchWidget, 3, 0, 1, 3);

    // 设备列表
    devTable = new QTableWidget;
    devTable->setColumnCount(4);
    devTable->setHorizontalHeaderLabels({"ID", "设备名称", "设备类型", "分组"});
    devTable->horizontalHeader()->setStretchLastSection(true);
    devLayout->addWidget(devTable, 4, 0, 1, 3);

    // 状态修改按钮
    QWidget *statusBtnWidget = new QWidget;
    QHBoxLayout *statusLayout = new QHBoxLayout(statusBtnWidget);
    statusLayout->setSpacing(5);

    QPushButton *btnOnline = new QPushButton("标记在线");
    btnOnline->setStyleSheet("background-color: #2ecc71; color: white; border-radius: 5px; padding: 5px;");
    statusLayout->addWidget(btnOnline);

    QPushButton *btnOffline = new QPushButton("标记离线");
    btnOffline->setStyleSheet("background-color: #e74c3c; color: white; border-radius: 5px; padding: 5px;");
    statusLayout->addWidget(btnOffline);

    QPushButton *btnError = new QPushButton("标记异常");
    btnError->setStyleSheet("background-color: #f39c12; color: white; border-radius: 5px; padding: 5px;");
    statusLayout->addWidget(btnError);

    devLayout->addWidget(statusBtnWidget, 5, 0, 1, 3);

    mainLayout->addWidget(devGroup);

    // ===== 2. 系统设置区域 =====
    QGroupBox *sysGroup = new QGroupBox("🔧 系统设置");
    sysGroup->setStyleSheet("QGroupBox { font-size: 16px; font-weight: bold; }");
    QGridLayout *sysLayout = new QGridLayout(sysGroup);
    sysLayout->setSpacing(15);

    sysLayout->addWidget(new QLabel("界面主题:"), 0, 0);
    themeCombo = new QComboBox;
    themeCombo->addItems({"默认主题", "浅色主题", "深色主题"});
    sysLayout->addWidget(themeCombo, 0, 1);

    sysLayout->addWidget(new QLabel("刷新频率(秒):"), 1, 0);
    refreshSpin = new QSpinBox;
    refreshSpin->setRange(1, 60);
    refreshSpin->setValue(5);
    sysLayout->addWidget(refreshSpin, 1, 1);

    sysLayout->addWidget(new QLabel("报警声音:"), 2, 0);
    soundCombo = new QComboBox;
    soundCombo->addItems({"提示音", "警报音", "叮咚音", "蜂鸣音"});
    sysLayout->addWidget(soundCombo, 2, 1);

    soundCheck = new QCheckBox("启用");
    soundCheck->setChecked(true);
    sysLayout->addWidget(soundCheck, 2, 2);

    testSoundBtn = new QPushButton("测试");
    testSoundBtn->setStyleSheet("background-color: #3498db; color: white; border-radius: 5px;");
    sysLayout->addWidget(testSoundBtn, 2, 3);

    backupBtn = new QPushButton("备份数据库");
    backupBtn->setStyleSheet("background-color: #9b59b6; color: white; border-radius: 5px;");
    sysLayout->addWidget(backupBtn, 0, 2);

    restoreBtn = new QPushButton("恢复数据库");
    restoreBtn->setStyleSheet("background-color: #f39c12; color: white; border-radius: 5px;");
    sysLayout->addWidget(restoreBtn, 1, 2);

    mainLayout->addWidget(sysGroup);

    // ===== 3. 按钮区域 =====
    QHBoxLayout *btnLayout = new QHBoxLayout;
    QPushButton *saveBtn = new QPushButton("保存设置");
    saveBtn->setStyleSheet("background-color: #27ae60; color: white; font-size: 14px; border-radius: 5px;");
    QPushButton *backBtn = new QPushButton("🏠 返回首页");
    backBtn->setStyleSheet("background-color: #95a5a6; color: white; font-size: 14px; border-radius: 5px;");

    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(backBtn);
    mainLayout->addLayout(btnLayout);

    // ===== 绑定信号 =====
    connect(searchBtn, &QPushButton::clicked, this, [=]() {
        QString keyword = searchEdit->text().trimmed();
        searchDevice(keyword);
    });

    connect(searchEdit, &QLineEdit::returnPressed, this, [=]() {
        QString keyword = searchEdit->text().trimmed();
        searchDevice(keyword);
    });

    connect(refreshBtn, &QPushButton::clicked, this, [=]() {
        searchEdit->clear();
        loadDeviceList();
    });

    // 状态标记
    connect(btnOnline, &QPushButton::clicked, this, [=]() {
        setDeviceStatus(1, "在线");
    });

    connect(btnOffline, &QPushButton::clicked, this, [=]() {
        setDeviceStatus(0, "离线");
    });

    connect(btnError, &QPushButton::clicked, this, [=]() {
        setDeviceStatus(2, "异常");
    });

    // 添加设备 逻辑部分
    connect(devAddBtn, &QPushButton::clicked, this, [=]() {
        QString name = devAddName->text().trimmed();
        // 关键：下拉框取选中值用 currentText()，不是 text()
        QString type = devAddType->currentData().toString().trimmed();
        QString group = devAddGroup->currentText();

        if (name.isEmpty()) {  // 类型是下拉框，不可能为空，只需校验名称
            QMessageBox::warning(this, "提示", "设备名称不能为空");
            return;
        }

        // 下拉框只能选固定值，这部分校验可以删掉（可选保留，防止后续改代码）
        // if(type!="灯光" && type!="空调" && type!="摄像头" && type!="窗帘")
        // {
        //     QMessageBox::warning(this,"提示",
        //                          "设备类型必须是:\n灯光\n空调\n摄像头\n窗帘");
        //     return;
        // }

        QSqlDatabase db = getDbConnection();
        if (!db.isOpen()) {
            QMessageBox::warning(this, "错误", "数据库连接失败");
            return;
        }

        QSqlQuery query(db);
        query.prepare("INSERT INTO devices(name, type, group_name, status) VALUES(?, ?, ?, 1)");
        query.addBindValue(name);
        query.addBindValue(type);
        query.addBindValue(group);

        // 在设备添加成功的代码块中，添加场景更新逻辑：
        if (query.exec()) {
            // ===== 新增：同步到 furniture =====
            QSqlDatabase db2 = getDbExtensionConnection();
            if (db2.open()) {
                QSqlQuery q2(db2);
                q2.prepare("INSERT INTO furniture(type,name,room,status,brightness,temperature,angle,openness,fault)"
                           "VALUES(?,?,?,?,?,?,?,?,0)");
                q2.addBindValue(type);
                q2.addBindValue(name);
                q2.addBindValue(group);
                q2.addBindValue(0);      // 默认离线
                q2.addBindValue(50);
                q2.addBindValue(26);
                q2.addBindValue(0);
                q2.addBindValue(50);

                if (q2.exec()) {
                    // 获取新添加设备的ID
                    int newDeviceId = q2.lastInsertId().toInt();

                    // 重新加载家具数据
                    loadFurnitureFromDb();

                    // ===== 新增：更新现有场景，新设备默认关闭 =====
                    bool scenesUpdated = false;
                    for (SceneMode &scene : sceneModes) {
                        // 新设备在所有现有场景中默认为关闭
                        if (!scene.deviceStatus.contains(newDeviceId)) {
                            scene.deviceStatus[newDeviceId] = false;
                            scenesUpdated = true;
                        }
                    }

                    if (scenesUpdated) {
                        saveSceneModes();  // 保存更新后的场景
                    }
                }

                db2.close();
            }

            updateDeviceStats();
            QMessageBox::information(this,"成功","设备添加成功");
            devAddName->clear();
            loadDeviceList();
        } else {
            QMessageBox::warning(this, "失败", "添加失败：" + query.lastError().text());
        }

        db.close();
    });

    // 删除设备
    connect(devDelBtn, &QPushButton::clicked, this, [=]() {
        int row = devTable->currentRow();
        if (row < 0) {
            QMessageBox::warning(this, "提示", "请选择要删除的设备");
            return;
        }

        QTableWidgetItem *idItem = devTable->item(row, 0);
        if (!idItem || idItem->text().isEmpty()) {
            QMessageBox::warning(this, "提示", "选中设备ID无效");
            return;
        }

        QString id = idItem->text();

        QSqlDatabase db = getDbConnection();
        if (!db.isOpen()) {
            QMessageBox::warning(this, "错误", "数据库连接失败");
            return;
        }

        QSqlQuery query(db);
        query.prepare("DELETE FROM devices WHERE id = ?");
        query.addBindValue(id);

        if (query.exec()) {
            // 同步删除 furniture
            QSqlDatabase db2 = getDbExtensionConnection();
            if (db2.open()) {

                QString name = devTable->item(row,1)->text();

                QSqlQuery q2(db2);
                q2.prepare("DELETE FROM furniture WHERE name=?");
                q2.addBindValue(name);
                q2.exec();

                db2.close();
            }

            loadFurnitureFromDb();
            updateDeviceStats();
            loadDeviceList();
        } else {
            QMessageBox::warning(this, "失败", "删除失败：" + query.lastError().text());
        }
        db.close();
    });

    // 测试连接
    connect(devTestBtn, &QPushButton::clicked, this, &MainWindow::testDeviceConn);

    // 主题切换
    connect(themeCombo, &QComboBox::currentIndexChanged, this, [=](int index) {
        applyTheme(index);
    });

    // 音效测试
    connect(testSoundBtn, &QPushButton::clicked, this, [=]() {
        playAlarmSound(false);
    });

    connect(soundCombo, &QComboBox::currentIndexChanged, this, [=](int) {
        updateSoundFile();
    });

    // 数据库操作
    connect(backupBtn, &QPushButton::clicked, this, &MainWindow::backupDatabase);
    connect(restoreBtn, &QPushButton::clicked, this, &MainWindow::restoreDatabase);

    // 保存设置和返回
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::saveSysSettings);
    connect(backBtn, &QPushButton::clicked, this, &MainWindow::backToHome);

    // 初始化
    alarmSound = new QSoundEffect(this);
    alarmSound->setVolume(1.0);
    updateSoundFile();
    loadSysSettings();
    loadDeviceList();

    stackedWidget->addWidget(sysManagePage);
}
// ==================== 系统管理功能函数 ====================

void MainWindow::loadDeviceList()
{
    devTable->setRowCount(0);
    QSqlDatabase db = getDbConnection();
    if (!db.open()) return;

    QSqlQuery query("SELECT id, name, type, group_name FROM devices ORDER BY id", db);
    while (query.next()) {
        int row = devTable->rowCount();
        devTable->insertRow(row);
        devTable->setItem(row, 0, new QTableWidgetItem(query.value(0).toString()));
        devTable->setItem(row, 1, new QTableWidgetItem(query.value(1).toString()));
        devTable->setItem(row, 2, new QTableWidgetItem(query.value(2).toString()));
        devTable->setItem(row, 3, new QTableWidgetItem(query.value(3).toString()));
    }
    db.close();
}

void MainWindow::searchDevice(const QString &keyword)
{
    if (keyword.isEmpty()) {
        loadDeviceList();
        return;
    }

    devTable->setRowCount(0);
    QSqlDatabase db = getDbConnection();
    if (!db.open()) return;

    QString sql = "SELECT id, name, type, group_name FROM devices "
                  "WHERE name LIKE ? OR type LIKE ? OR group_name LIKE ? "
                  "ORDER BY id";

    QString searchPattern = "%" + keyword + "%";
    QSqlQuery query(db);
    query.prepare(sql);
    query.addBindValue(searchPattern);
    query.addBindValue(searchPattern);
    query.addBindValue(searchPattern);

    if (query.exec()) {
        while (query.next()) {
            int row = devTable->rowCount();
            devTable->insertRow(row);
            devTable->setItem(row, 0, new QTableWidgetItem(query.value(0).toString()));
            devTable->setItem(row, 1, new QTableWidgetItem(query.value(1).toString()));
            devTable->setItem(row, 2, new QTableWidgetItem(query.value(2).toString()));
            devTable->setItem(row, 3, new QTableWidgetItem(query.value(3).toString()));
        }

        if (devTable->rowCount() == 0) {
            QMessageBox::information(this, "结果", "未找到包含「" + keyword + "」的设备");
        }
    }
    db.close();
}
void MainWindow::setDeviceStatus(int statusCode, const QString &statusText)
{
    int row = devTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "提示", "请先选择要修改状态的设备");
        return;
    }

    QTableWidgetItem *idItem = devTable->item(row, 0);
    if (!idItem || idItem->text().isEmpty()) {
        QMessageBox::warning(this, "提示", "选中设备ID无效");
        return;
    }

    QString devId = idItem->text();
    QString devName = devTable->item(row,1)->text();

    QSqlDatabase db = getDbConnection();
    if (!db.open()) {
        QMessageBox::warning(this, "错误", "数据库连接失败");
        return;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE devices SET status = ? WHERE id = ?");
    query.addBindValue(statusCode);
    query.addBindValue(devId);

    if (query.exec()) {

        // ===== 新增：同步到 furniture 表 =====
        QSqlDatabase db2 = getDbExtensionConnection();
        if (db2.open()) {

            QSqlQuery q2(db2);

            int realStatus = 0;
            int fault = 0;

            if(statusCode == 1)      // 在线
            {
                realStatus = 1;
                fault = 0;
            }
            else if(statusCode == 0) // 离线
            {
                realStatus = 0;
                fault = 0;
            }
            else if(statusCode == 2) // 异常
            {
                realStatus = 0;
                fault = 1;
            }

            q2.prepare("UPDATE furniture SET status=?, fault=? WHERE name=?");
            q2.addBindValue(realStatus);
            q2.addBindValue(fault);
            q2.addBindValue(devName);
            q2.exec();

            db2.close();
        }

        // 重新加载家具设备
        loadFurnitureFromDb();

        // ===== 新增：更新首页客厅设备按钮样式 =====
        Furniture* livingRoomLight = getLivingRoomLight();
        Furniture* livingRoomAircon = getLivingRoomAircon();

        // 如果修改的是客厅灯，更新首页按钮样式
        if (livingRoomLight && livingRoomLight->name == devName) {
            if (livingRoomLight->fault) {
                livingRoomLightStatusLabel->setText("客厅灯: 故障");
                livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #e74c3c; font-weight: bold;");
                livingRoomLightBrightnessLabel->setText("亮度: --");
                livingRoomLightBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-size: 14px; border-radius: 5px; }"
                                                  "QPushButton:hover { background-color: #c0392b; }");
            } else if (livingRoomLight->status) {
                livingRoomLightStatusLabel->setText("客厅灯: 开启");
                livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #f39c12;");
                livingRoomLightBrightnessLabel->setText(QString("亮度: %1%").arg(livingRoomLight->brightness));
                livingRoomLightBtn->setStyleSheet("QPushButton { background-color: #f1c40f; font-size: 14px; border-radius: 5px; }"
                                                  "QPushButton:hover { background-color: #f39c12; }");
            } else {
                livingRoomLightStatusLabel->setText("客厅灯: 关闭");
                livingRoomLightStatusLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");
                livingRoomLightBrightnessLabel->setText("亮度: --");
                livingRoomLightBtn->setStyleSheet("QPushButton { background-color: #f1c40f; font-size: 14px; border-radius: 5px; }"
                                                  "QPushButton:hover { background-color: #f39c12; }");
            }
        }

        // 如果修改的是客厅空调，更新首页按钮样式
        if (livingRoomAircon && livingRoomAircon->name == devName) {
            if (livingRoomAircon->fault) {
                livingRoomAirconStatusLabel->setText("客厅空调: 故障");
                livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #e74c3c; font-weight: bold;");
                livingRoomAirconTempLabel->setText("温度: --");
                livingRoomAirconBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-size: 14px; border-radius: 5px; }"
                                                   "QPushButton:hover { background-color: #c0392b; }");
            } else if (livingRoomAircon->status) {
                livingRoomAirconStatusLabel->setText("客厅空调: 开启");
                livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #3498db;");
                livingRoomAirconTempLabel->setText(QString("温度: %1°C").arg(livingRoomAircon->temperature));
                livingRoomAirconBtn->setStyleSheet("QPushButton { background-color: #3498db; color: white; font-size: 14px; border-radius: 5px; }"
                                                   "QPushButton:hover { background-color: #2980b9; }");
            } else {
                livingRoomAirconStatusLabel->setText("客厅空调: 关闭");
                livingRoomAirconStatusLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");
                livingRoomAirconTempLabel->setText("温度: --");
                livingRoomAirconBtn->setStyleSheet("QPushButton { background-color: #3498db; color: white; font-size: 14px; border-radius: 5px; }"
                                                   "QPushButton:hover { background-color: #2980b9; }");
            }
        }

        updateDeviceStats();

        // ===== 原来的弹窗 =====
        QMessageBox::information(this, "成功",
                                 QString("设备已标记为：%1").arg(statusText));

        loadDeviceList();

        if (soundCheck->isChecked()) {
            if (statusCode == 2) {
                playAlarmSound(true);
            } else if (statusCode == 0) {
                playAlarmSound(false);
            }
        }

    } else {
        QMessageBox::warning(this, "失败",
                             QString("修改状态失败：%1").arg(query.lastError().text()));
    }

    db.close();
}

void MainWindow::testDeviceConn()
{
    int row = devTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "提示", "请选择要测试的设备");
        return;
    }

    QString devName = devTable->item(row, 1)->text();
    int devId = devTable->item(row, 0)->text().toInt();

    // 检查设备是否异常
    Furniture* f = getFurnitureById(devId);
    if (f && f->fault == 1) {
        QMessageBox::warning(this, "测试结果",
                             QString("%1\n❌ 设备连接异常！\n请检查设备故障状态").arg(devName));
        return;
    }

    QMessageBox::information(this, "测试结果", devName + "\n✅ 设备连接正常");
}
void MainWindow::backupDatabase()
{
    QString backupDir = QFileDialog::getExistingDirectory(this, "选择备份目录", "",
                                                          QFileDialog::ShowDirsOnly);
    if (backupDir.isEmpty()) return;

    // 生成备份文件名（包含时间戳）
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString mainDbBackup = backupDir + "/smarthome_" + timestamp + ".db";
    QString extDbBackup = backupDir + "/smart_home_extension_" + timestamp + ".db";

    // 备份主数据库
    bool mainSuccess = QFile::copy("smarthome.db", mainDbBackup);

    // 备份扩展数据库
    bool extSuccess = QFile::copy("smart_home_extension.db", extDbBackup);

    if (mainSuccess && extSuccess) {
        QMessageBox::information(this, "成功",
                                 QString("数据库备份完成！\n\n主数据库：%1\n扩展数据库：%2")
                                     .arg(mainDbBackup).arg(extDbBackup));
    } else {
        QString errorMsg;
        if (!mainSuccess) errorMsg += "主数据库备份失败\n";
        if (!extSuccess) errorMsg += "扩展数据库备份失败\n";
        QMessageBox::warning(this, "失败", errorMsg);
    }
}

void MainWindow::restoreDatabase()
{
    QString mainDbPath = QFileDialog::getOpenFileName(this, "选择要恢复的主数据库文件",
                                                      "", "DB Files (*.db)");
    if (mainDbPath.isEmpty()) return;

    // 根据主数据库文件名生成对应的扩展数据库文件名
    QFileInfo fileInfo(mainDbPath);
    QString baseName = fileInfo.baseName();
    QString extDbPath = fileInfo.path() + "/" + baseName.replace("smarthome", "smart_home_extension") + ".db";

    // 检查扩展数据库文件是否存在
    if (!QFile::exists(extDbPath)) {
        // 如果找不到对应的扩展数据库文件，让用户手动选择
        extDbPath = QFileDialog::getOpenFileName(this, "请选择对应的扩展数据库文件",
                                                 fileInfo.path(), "DB Files (*.db)");
        if (extDbPath.isEmpty()) return;
    }

    // 关闭所有数据库连接
    if (QSqlDatabase::contains("qt_sql_default_connection")) {
        QSqlDatabase::removeDatabase("qt_sql_default_connection");
    }
    if (QSqlDatabase::contains("smart_home_extension")) {
        QSqlDatabase::removeDatabase("smart_home_extension");
    }

    // 恢复主数据库
    QFile::remove("smarthome.db");
    bool mainSuccess = QFile::copy(mainDbPath, "smarthome.db");

    // 恢复扩展数据库
    QFile::remove("smart_home_extension.db");
    bool extSuccess = QFile::copy(extDbPath, "smart_home_extension.db");

    if (mainSuccess && extSuccess) {
        // 重新初始化数据库
        initDb();
        initExtensionDb();

        // 重新加载数据
        loadFurnitureFromDb();
        loadDeviceList();
        updateDeviceStats();

        QMessageBox::information(this, "成功", "数据库恢复完成！");
    } else {
        QString errorMsg;
        if (!mainSuccess) errorMsg += "主数据库恢复失败\n";
        if (!extSuccess) errorMsg += "扩展数据库恢复失败\n";
        QMessageBox::warning(this, "失败", errorMsg);
    }
}

void MainWindow::saveSysSettings()
{
    QSettings settings("SmartHome", "Settings");
    settings.setValue("theme", themeCombo->currentIndex());
    settings.setValue("refreshInterval", refreshSpin->value());
    settings.setValue("alarmSound", soundCheck->isChecked());
    settings.setValue("soundType", soundCombo->currentIndex());

    QMessageBox::information(this, "成功", "系统设置已保存");
}

void MainWindow::loadSysSettings()
{
    QSettings settings("SmartHome", "Settings");
    themeCombo->setCurrentIndex(settings.value("theme", 0).toInt());
    refreshSpin->setValue(settings.value("refreshInterval", 5).toInt());
    soundCheck->setChecked(settings.value("alarmSound", true).toBool());
    soundCombo->setCurrentIndex(settings.value("soundType", 0).toInt());
}

void MainWindow::updateSoundFile()
{
    QString soundFile;
    switch (soundCombo->currentIndex()) {
    case 0: soundFile = "qrc:/sounds/tip.wav"; break;
    case 1: soundFile = "qrc:/sounds/alarm.wav"; break;
    case 2: soundFile = "qrc:/sounds/ding.wav"; break;
    case 3: soundFile = "qrc:/sounds/beep.wav"; break;
    }
    alarmSound->setSource(QUrl(soundFile));
}

void MainWindow::playAlarmSound(bool isLoop)
{
    if (!soundCheck->isChecked()) {
        QMessageBox::warning(this, "提示", "请先启用报警声音开关");
        return;
    }

    updateSoundFile();

    if (alarmSound->source().isEmpty()) {
        QMessageBox::critical(this, "错误", "音效文件路径为空");
        return;
    }

    alarmSound->setLoopCount(isLoop ? QSoundEffect::Infinite : 1);
    alarmSound->stop();
    alarmSound->play();

    if (isLoop) {
        QMessageBox::information(this, "提示", "设备异常！报警音将持续播放");
        alarmSound->stop();
    }
}

void MainWindow::applyTheme(int themeIndex)
{
    // ======================
    // ① 默认模式：微软雅黑 + 清晰正式
    // ======================
    QString defaultStyle = R"(
        QMainWindow { background-color: #f5f5f5; }
        QWidget {
            font-family: "Microsoft YaHei", "微软雅黑";
            font-size: 14px;
            font-weight: normal;
            color: #212529;
        }

        QGroupBox {
            font-weight: bold;
            border: 1px solid #ddd;
            border-radius: 6px;
            margin-top: 10px;
            padding-top: 10px;
            padding-left: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 8px 0 8px;
            font-family: "Microsoft YaHei", "微软雅黑";
            font-size: 15px;
            font-weight: bold;
        }

        QPushButton {
            border: none;
            padding: 8px 15px;
            border-radius: 6px;
            background-color: #007bff;
            color: white;
            font-family: "Microsoft YaHei", "微软雅黑";
            font-size: 14px;
            font-weight: 500;
            transition: all 0.2s ease;
        }
        QPushButton:hover {
            background-color: #0069d9;
            transform: scale(1.03);
        }
        QPushButton:pressed {
            background-color: #005cbf;
            transform: scale(0.97);
        }

        QLineEdit, QComboBox, QSpinBox {
            padding: 8px 10px;
            border: 1px solid #ddd;
            border-radius: 6px;
            background-color: white;
            font-family: "Microsoft YaHei", "微软雅黑";
            font-size: 14px;
            transition: all 0.2s ease;
        }
        QLineEdit:focus, QComboBox:focus {
            border-color: #80bdff;
            box-shadow: 0 0 8px rgba(128, 189, 255, 0.25);
            outline: none;
        }

        QComboBox QAbstractItemView {
            border: 1px solid #ddd;
            border-radius: 6px;
            background-color: white;
            color: #333;
            font-family: "Microsoft YaHei", "微软雅黑";
            font-size: 14px;
            selection-background-color: #007bff;
            selection-color: white;
        }

        QSlider::groove:horizontal {
            height: 8px;
            background-color: #e9ecef;
            border-radius: 4px;
        }
        QSlider::handle:horizontal {
            background-color: #007bff;
            width: 20px;
            height: 20px;
            margin: -6px 0;
            border-radius: 10px;
            transition: all 0.15s ease;
        }

        QCheckBox {
            spacing: 8px;
            font-family: "Microsoft YaHei", "微软雅黑";
            font-size: 14px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 1px solid #ddd;
            border-radius: 4px;
            background-color: white;
        }
        QCheckBox::indicator:checked {
            background-color: #007bff;
            border-color: #007bff;
        }

        QTableWidget {
            gridline-color: #ddd;
            background-color: white;
            border-radius: 6px;
        }
        QHeaderView::section {
            background-color: #eee;
            padding: 8px;
            font-family: "Microsoft YaHei", "微软雅黑";
            font-size: 14px;
            font-weight: bold;
        }

        QMessageBox {
            background-color: white;
            font-family: "Microsoft YaHei", "微软雅黑";
            font-size: 14px;
        }
    )";

    // ======================
    // ② 浅色模式：Arial / Segoe UI + 轻盈干净
    // ======================
    QString lightStyle = R"(
        QMainWindow { background-color: #ffffff; }
        QWidget {
            font-family: "Segoe UI", "Arial", "Helvetica";
            font-size: 14px;
            font-weight: normal;
            color: #333333;
        }

        QGroupBox {
            font-weight: 500;
            border: 1px solid #eee;
            border-radius: 6px;
            margin-top: 10px;
            padding-top: 10px;
            padding-left: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 8px 0 8px;
            font-family: "Segoe UI", "Arial";
            font-size: 15px;
            font-weight: 500;
            color: #444;
        }

        QPushButton {
            border: none;
            padding: 8px 15px;
            border-radius: 6px;
            background-color: #f8f8f8;
            color: #333;
            font-family: "Segoe UI", "Arial";
            font-size: 14px;
            font-weight: 500;
            transition: all 0.2s ease;
        }
        QPushButton:hover {
            background-color: #e8e8e8;
            transform: scale(1.03);
        }
        QPushButton:pressed {
            background-color: #e0e0e0;
            transform: scale(0.97);
        }

        QLineEdit, QComboBox, QSpinBox {
            padding: 8px 10px;
            border: 1px solid #eee;
            border-radius: 6px;
            background-color: #fafafa;
            font-family: "Segoe UI", "Arial";
            font-size: 14px;
            transition: all 0.2s ease;
        }
        QLineEdit:focus, QComboBox:focus {
            border-color: #bdbdbd;
            box-shadow: 0 0 8px rgba(0,0,0,0.08);
            outline: none;
        }

        QComboBox QAbstractItemView {
            border: 1px solid #eee;
            border-radius: 6px;
            background-color: white;
            color: #333;
            font-family: "Segoe UI", "Arial";
            font-size: 14px;
            selection-background-color: #e0f2f1;
            selection-color: #263238;
        }

        QSlider::groove:horizontal {
            height: 8px;
            background-color: #f5f5f5;
            border-radius: 4px;
        }
        QSlider::handle:horizontal {
            background-color: #90a4ae;
            width: 20px;
            height: 20px;
            margin: -6px 0;
            border-radius: 10px;
            transition: all 0.15s ease;
        }

        QCheckBox {
            spacing: 8px;
            font-family: "Segoe UI", "Arial";
            font-size: 14px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 1px solid #eee;
            border-radius: 4px;
            background-color: white;
        }
        QCheckBox::indicator:checked {
            background-color: #80cbc4;
            border-color: #80cbc4;
        }

        QTableWidget {
            gridline-color: #eee;
            background-color: white;
            border-radius: 6px;
        }
        QHeaderView::section {
            background-color: #f8f8f8;
            padding: 8px;
            font-family: "Segoe UI", "Arial";
            font-size: 14px;
            font-weight: 500;
        }

        QMessageBox {
            background-color: #fafafa;
            font-family: "Segoe UI", "Arial";
            font-size: 14px;
        }
    )";

    // ======================
    // ③ 深色模式：Roboto / 等宽清晰 + 高级科技感
    // ======================
    QString darkStyle = R"(
        QMainWindow { background-color: #2c3e50; }
        QWidget {
            font-family: "Roboto", "Microsoft YaHei", "等线";
            font-size: 14px;
            font-weight: 500;
            color: #ecf0f1;
        }

        QGroupBox {
            font-weight: bold;
            border: 1px solid #34495e;
            border-radius: 6px;
            margin-top: 10px;
            padding-top: 10px;
            padding-left: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 8px 0 8px;
            font-family: "Roboto", "Microsoft YaHei";
            font-size: 15px;
            font-weight: bold;
            color: #fff;
        }

        QPushButton {
            border: none;
            padding: 8px 15px;
            border-radius: 6px;
            background-color: #34495e;
            color: #ecf0f1;
            font-family: "Roboto", "Microsoft YaHei";
            font-size: 14px;
            font-weight: 500;
            transition: all 0.2s ease;
        }
        QPushButton:hover {
            background-color: #4a6584;
            transform: scale(1.03);
        }
        QPushButton:pressed {
            background-color: #1f2d3d;
            transform: scale(0.97);
        }

        QLineEdit, QComboBox, QSpinBox {
            padding: 8px 10px;
            border: 1px solid #34495e;
            border-radius: 6px;
            background-color: #1a2530;
            color: #ecf0f1;
            font-family: "Roboto", "Microsoft YaHei";
            font-size: 14px;
            transition: all 0.2s ease;
        }
        QLineEdit:focus, QComboBox:focus {
            border-color: #7f8c8d;
            box-shadow: 0 0 8px rgba(127,140,141,0.25);
            outline: none;
        }

        QComboBox QAbstractItemView {
            border: 1px solid #34495e;
            border-radius: 6px;
            background-color: #1a2530;
            color: #ecf0f1;
            font-family: "Roboto", "Microsoft YaHei";
            font-size: 14px;
            selection-background-color: #4a6584;
            selection-color: white;
        }

        QSlider::groove:horizontal {
            height: 8px;
            background-color: #34495e;
            border-radius: 4px;
        }
        QSlider::handle:horizontal {
            background-color: #6abcff;
            width: 20px;
            height: 20px;
            margin: -6px 0;
            border-radius: 10px;
            transition: all 0.15s ease;
        }

        QCheckBox {
            spacing: 8px;
            font-family: "Roboto", "Microsoft YaHei";
            font-size: 14px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 1px solid #34495e;
            border-radius: 4px;
            background-color: #1a2530;
        }
        QCheckBox::indicator:checked {
            background-color: #6abcff;
            border-color: #6abcff;
        }

        QTableWidget {
            gridline-color: #34495e;
            background-color: #1a2530;
            border-radius: 6px;
        }
        QHeaderView::section {
            background-color: #34495e;
            padding: 8px;
            color: #ecf0f1;
            font-family: "Roboto", "Microsoft YaHei";
            font-size: 14px;
            font-weight: bold;
        }

        QMessageBox, QDialog {
            background-color: #34495e;
            color: #ecf0f1;
            font-family: "Roboto", "Microsoft YaHei";
            font-size: 14px;
        }
        QMessageBox QLabel, QDialog QLabel {
            color: #ecf0f1;
        }
    )";

    switch (themeIndex) {
    case 0: this->setStyleSheet(defaultStyle); break;
    case 1: this->setStyleSheet(lightStyle); break;
    case 2: this->setStyleSheet(darkStyle); break;
    }

    // 恢复特殊按钮样式
    QList<QPushButton*> specialBtns = {
        livingRoomLightBtn, livingRoomAirconBtn, homeModeBtn, sleepModeBtn, logoutBtn,
        devAddBtn, devDelBtn, devTestBtn, backupBtn, restoreBtn
    };
    for (QPushButton *btn : specialBtns) {
        if (btn) {
            btn->setStyleSheet(btn->styleSheet());
        }
    }
}

// ==================== MainWindow 类的成员函数 ====================
QSqlDatabase MainWindow::getDbConnection()
{
    return ::getDbConnection();  // 调用全局静态函数
}
// ========== HTTP 服务：启动函数 ==========
void MainWindow::startHttpServer()
{
    httpServer = new QTcpServer(this);
    // 监听 8080 端口（和 TCP 的 8888 区分，避免端口冲突）
    bool isListening = httpServer->listen(QHostAddress::Any, 8080);
    if (isListening) {
        qDebug() << "HTTP 服务启动成功，监听端口：8080";
        // 绑定新连接信号
        connect(httpServer, &QTcpServer::newConnection, this, &MainWindow::onHttpNewConnection);
    } else {
        qDebug() << "HTTP 服务启动失败：" << httpServer->errorString();
        QMessageBox::warning(this, "错误", "HTTP 服务启动失败，端口 8080 可能被占用！");
    }
}

// ========== HTTP 服务：处理新连接 ==========
// ========== 处理 HTTP 新连接（所有逻辑都在这里） ==========
void MainWindow::onHttpNewConnection()
{
    QTcpSocket *httpSocket = httpServer->nextPendingConnection();
    if (!httpSocket) return;

    // ========== 新增：创建超时定时器 ==========
    QTimer *timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true); // 仅触发一次
    timeoutTimer->setInterval(5000);   // 超时时间：5秒（可调整）

    // 绑定超时触发逻辑
    connect(timeoutTimer, &QTimer::timeout, this, [=]() {
        if (httpSocket->state() == QTcpSocket::ConnectedState) {
            qDebug() << "HTTP 请求超时：" << httpSocket->peerAddress().toString();

            // 构造超时提示页面（和成功页面风格统一）
            QString timeoutHtml = R"(
                <!DOCTYPE html>
                <html lang="zh-CN">
                <head>
                    <meta charset="UTF-8">
                    <meta name="viewport" content="width=device-width, initial-scale=1.0">
                    <title>请求超时</title>
                    <style>
                        * { margin: 0; padding: 0; box-sizing: border-box; font-family: "Microsoft YaHei", Arial, sans-serif; }
                        body {
                            background-color: #f5f7fa;
                            display: flex;
                            flex-direction: column;
                            align-items: center;
                            justify-content: center;
                            min-height: 100vh;
                            padding: 20px;
                        }
                        .timeout-card {
                            background-color: white;
                            border-radius: 16px;
                            padding: 40px 30px;
                            box-shadow: 0 8px 24px rgba(0,0,0,0.1);
                            text-align: center;
                            max-width: 500px;
                            width: 100%;
                        }
                        .icon {
                            font-size: 80px;
                            color: #e74c3c;
                            margin-bottom: 20px;
                        }
                        .timeout-title {
                            font-size: 36px;
                            color: #2c3e50;
                            margin-bottom: 15px;
                            font-weight: bold;
                        }
                        .timeout-desc {
                            font-size: 20px;
                            color: #7f8c8d;
                            margin-bottom: 30px;
                        }
                        .retry-btn {
                            display: inline-block;
                            padding: 12px 30px;
                            background-color: #3498db;
                            color: white;
                            font-size: 18px;
                            font-weight: 600;
                            text-decoration: none;
                            border-radius: 8px;
                            transition: all 0.3s ease;
                        }
                        .retry-btn:hover {
                            transform: translateY(-3px);
                            box-shadow: 0 6px 15px rgba(0,0,0,0.1);
                        }
                    </style>
                    <link rel="stylesheet" href="https://cdn.bootcdn.net/ajax/libs/font-awesome/6.4.0/css/all.min.css">
                </head>
                <body>
                    <div class="timeout-card">
                        <div class="icon"><i class="fas fa-exclamation-circle"></i></div>
                        <h1 class="timeout-title">请求超时</h1>
                        <p class="timeout-desc">服务器响应超时，请检查网络或重试</p>
                        <a href="/" class="retry-btn"><i class="fas fa-refresh"></i> 返回控制页面</a>
                    </div>
                </body>
                </html>
            )";

            // 发送超时响应
            QByteArray response;
            response.append("HTTP/1.1 504 Gateway Timeout\r\n");
            response.append("Content-Type: text/html; charset=utf-8\r\n");
            response.append(QString("Content-Length: %1\r\n\r\n").arg(timeoutHtml.toUtf8().length()).toUtf8());
            response.append(timeoutHtml.toUtf8());

            httpSocket->write(response);
            httpSocket->flush();
        }

        // 关闭连接并释放资源
        httpSocket->disconnectFromHost();
        httpSocket->deleteLater();
        timeoutTimer->deleteLater(); // 释放定时器
    });

    // ========== 原有：处理 HTTP 请求 ==========
    connect(httpSocket, &QTcpSocket::readyRead, this, [=]() {
        // 收到请求后立即停止超时定时器（避免误触发）
        timeoutTimer->stop();

        // 1. 读取请求数据
        QByteArray requestData = httpSocket->readAll();
        qDebug() << "收到 HTTP 请求：" << requestData.left(100);

        // 2. 定义响应内容
        QString responseBody;
        QString httpStatus = "200 OK";

        // 3. 处理功能指令（/1~/4）
        if (requestData.contains("GET /1")) {
            sceneHome(); // 回家模式
            QString faultyDevices;
            for (const Furniture &f : m_furniture) {
                if (f.fault == 1) {
                    if (!faultyDevices.isEmpty()) faultyDevices += "、";
                    faultyDevices += f.name;
                }
            }

            if (faultyDevices.isEmpty()) {


            responseBody = R"(
                <!DOCTYPE html>
                <html lang="zh-CN">
                <head>
                    <meta charset="UTF-8">
                    <meta name="viewport" content="width=device-width, initial-scale=1.0">
                    <title>操作成功</title>
                    <style>
                        * { margin: 0; padding: 0; box-sizing: border-box; font-family: "Microsoft YaHei", Arial, sans-serif; }
                        body {
                            background-color: #f5f7fa;
                            display: flex;
                            flex-direction: column;
                            align-items: center;
                            justify-content: center;
                            min-height: 100vh;
                            padding: 20px;
                        }
                        .success-card {
                            background-color: white;
                            border-radius: 16px;
                            padding: 40px 30px;
                            box-shadow: 0 8px 24px rgba(0,0,0,0.1);
                            text-align: center;
                            max-width: 500px;
                            width: 100%;
                        }
                        .icon {
                            font-size: 80px;
                            color: #f39c12;
                            margin-bottom: 20px;
                        }
                        .success-title {
                            font-size: 36px;
                            color: #2c3e50;
                            margin-bottom: 15px;
                            font-weight: bold;
                        }
                        .success-desc {
                            font-size: 20px;
                            color: #7f8c8d;
                            margin-bottom: 30px;
                        }
                        .countdown {
                            font-size: 18px;
                            color: #f39c12;
                            font-weight: 600;
                        }
                    </style>
                    <link rel="stylesheet" href="https://cdn.bootcdn.net/ajax/libs/font-awesome/6.4.0/css/all.min.css">
                </head>
                <body>
                    <div class="success-card">
                        <div class="icon"><i class="fas fa-check-circle"></i></div>
                        <h1 class="success-title">回家模式已启动</h1>
                        <p class="success-desc">操作已生效，设备状态已更新</p>
                        <p class="countdown">1秒后自动返回控制页面...</p>
                    </div>
                    <meta http-equiv="refresh" content="1;url=/">
                </body>
                </html>
            )";}
            else{
                // 执行失败页面（红色警告，提示重试）
                        responseBody = R"(
                            <!DOCTYPE html>
                            <html lang="zh-CN">
                            <head>
                                <meta charset="UTF-8">
                                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                                <title>操作失败</title>
                                <style>
                                    * { margin: 0; padding: 0; box-sizing: border-box; font-family: "Microsoft YaHei", Arial, sans-serif; }
                                    body {
                                        background-color: #f5f7fa;
                                        display: flex;
                                        flex-direction: column;
                                        align-items: center;
                                        justify-content: center;
                                        min-height: 100vh;
                                        padding: 20px;
                                    }
                                    .fail-card {
                                        background-color: white;
                                        border-radius: 16px;
                                        padding: 40px 30px;
                                        box-shadow: 0 8px 24px rgba(0,0,0,0.1);
                                        text-align: center;
                                        max-width: 500px;
                                        width: 100%;
                                    }
                                    .icon {
                                        font-size: 80px;
                                        color: #e74c3c; /* 红色：失败 */
                                        margin-bottom: 20px;
                                    }
                                    .fail-title {
                                        font-size: 36px;
                                        color: #2c3e50;
                                        margin-bottom: 15px;
                                        font-weight: bold;
                                    }
                                    .fail-desc {
                                        font-size: 20px;
                                        color: #7f8c8d;
                                        margin-bottom: 30px;
                                    }
                                    .retry-btn {
                                        display: inline-block;
                                        padding: 12px 30px;
                                        background-color: #3498db;
                                        color: white;
                                        font-size: 18px;
                                        font-weight: 600;
                                        text-decoration: none;
                                        border-radius: 8px;
                                        transition: all 0.3s ease;
                                    }
                                    .retry-btn:hover {
                                        transform: translateY(-3px);
                                        box-shadow: 0 6px 15px rgba(0,0,0,0.1);
                                    }
                                </style>
                                <link rel="stylesheet" href="https://cdn.bootcdn.net/ajax/libs/font-awesome/6.4.0/css/all.min.css">
                            </head>
                            <body>
                                <div class="fail-card">
                                    <div class="icon"><i class="fas fa-exclamation-circle"></i></div>
                                    <h1 class="fail-title">回家模式启动失败 ❌</h1>
                                    <p class="fail-desc">部分设备控制失败，请检查控件是否存在或重试</p>
                                    <a href="/1" class="retry-btn"><i class="fas fa-refresh"></i> 重新尝试</a>
                                </div>
                            </body>
                            </html>
                        )";

            }

        } else if (requestData.contains("GET /2")) {
            sceneSleep(); // 睡眠模式
            QString faultyDevices;
            for (const Furniture &f : m_furniture) {
                if (f.type == "camera" && f.fault == 1) {
                    if (!faultyDevices.isEmpty()) faultyDevices += "、";
                    faultyDevices += f.name;
                }
            }
            if (faultyDevices.isEmpty()) {responseBody = R"(
                <!DOCTYPE html>
                <html lang="zh-CN">
                <head>
                    <meta charset="UTF-8">
                    <meta name="viewport" content="width=device-width, initial-scale=1.0">
                    <title>操作成功</title>
                    <style>
                        * { margin: 0; padding: 0; box-sizing: border-box; font-family: "Microsoft YaHei", Arial, sans-serif; }
                        body {
                            background-color: #f5f7fa;
                            display: flex;
                            flex-direction: column;
                            align-items: center;
                            justify-content: center;
                            min-height: 100vh;
                            padding: 20px;
                        }
                        .success-card {
                            background-color: white;
                            border-radius: 16px;
                            padding: 40px 30px;
                            box-shadow: 0 8px 24px rgba(0,0,0,0.1);
                            text-align: center;
                            max-width: 500px;
                            width: 100%;
                        }
                        .icon {
                            font-size: 80px;
                            color: #3498db;
                            margin-bottom: 20px;
                        }
                        .success-title {
                            font-size: 36px;
                            color: #2c3e50;
                            margin-bottom: 15px;
                            font-weight: bold;
                        }
                        .success-desc {
                            font-size: 20px;
                            color: #7f8c8d;
                            margin-bottom: 30px;
                        }
                        .countdown {
                            font-size: 18px;
                            color: #3498db;
                            font-weight: 600;
                        }
                    </style>
                    <link rel="stylesheet" href="https://cdn.bootcdn.net/ajax/libs/font-awesome/6.4.0/css/all.min.css">
                </head>
                <body>
                    <div class="success-card">
                        <div class="icon"><i class="fas fa-check-circle"></i></div>
                        <h1 class="success-title">睡眠模式已启动</h1>
                        <p class="success-desc">操作已生效，设备状态已更新</p>
                        <p class="countdown">1秒后自动返回控制页面...</p>
                    </div>
                    <meta http-equiv="refresh" content="1;url=/">
                </body>
                </html>
            )";}
            else{
                // 执行失败页面（红色警告，提示重试）
                        responseBody = R"(
                            <!DOCTYPE html>
                            <html lang="zh-CN">
                            <head>
                                <meta charset="UTF-8">
                                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                                <title>操作失败</title>
                                <style>
                                    * { margin: 0; padding: 0; box-sizing: border-box; font-family: "Microsoft YaHei", Arial, sans-serif; }
                                    body {
                                        background-color: #f5f7fa;
                                        display: flex;
                                        flex-direction: column;
                                        align-items: center;
                                        justify-content: center;
                                        min-height: 100vh;
                                        padding: 20px;
                                    }
                                    .fail-card {
                                        background-color: white;
                                        border-radius: 16px;
                                        padding: 40px 30px;
                                        box-shadow: 0 8px 24px rgba(0,0,0,0.1);
                                        text-align: center;
                                        max-width: 500px;
                                        width: 100%;
                                    }
                                    .icon {
                                        font-size: 80px;
                                        color: #e74c3c; /* 红色：失败 */
                                        margin-bottom: 20px;
                                    }
                                    .fail-title {
                                        font-size: 36px;
                                        color: #2c3e50;
                                        margin-bottom: 15px;
                                        font-weight: bold;
                                    }
                                    .fail-desc {
                                        font-size: 20px;
                                        color: #7f8c8d;
                                        margin-bottom: 30px;
                                    }
                                    .retry-btn {
                                        display: inline-block;
                                        padding: 12px 30px;
                                        background-color: #3498db;
                                        color: white;
                                        font-size: 18px;
                                        font-weight: 600;
                                        text-decoration: none;
                                        border-radius: 8px;
                                        transition: all 0.3s ease;
                                    }
                                    .retry-btn:hover {
                                        transform: translateY(-3px);
                                        box-shadow: 0 6px 15px rgba(0,0,0,0.1);
                                    }
                                </style>
                                <link rel="stylesheet" href="https://cdn.bootcdn.net/ajax/libs/font-awesome/6.4.0/css/all.min.css">
                            </head>
                            <body>
                                <div class="fail-card">
                                    <div class="icon"><i class="fas fa-exclamation-circle"></i></div>
                                    <h1 class="fail-title">睡眠模式启动失败 ❌</h1>
                                    <p class="fail-desc">部分设备控制失败，请检查控件是否存在或重试</p>
                                    <a href="/1" class="retry-btn"><i class="fas fa-refresh"></i> 重新尝试</a>
                                </div>
                            </body>
                            </html>
                        )";

            }
        } else if (requestData.contains("GET /3")) {
            controlCameraById(7); // 开监控

            responseBody = R"(
                <!DOCTYPE html>
                <html lang="zh-CN">
                <head>
                    <meta charset="UTF-8">
                    <meta name="viewport" content="width=device-width, initial-scale=1.0">
                    <title>操作成功</title>
                    <style>
                        * { margin: 0; padding: 0; box-sizing: border-box; font-family: "Microsoft YaHei", Arial, sans-serif; }
                        body {
                            background-color: #f5f7fa;
                            display: flex;
                            flex-direction: column;
                            align-items: center;
                            justify-content: center;
                            min-height: 100vh;
                            padding: 20px;
                        }
                        .success-card {
                            background-color: white;
                            border-radius: 16px;
                            padding: 40px 30px;
                            box-shadow: 0 8px 24px rgba(0,0,0,0.1);
                            text-align: center;
                            max-width: 500px;
                            width: 100%;
                        }
                        .icon {
                            font-size: 80px;
                            color: #e74c3c;
                            margin-bottom: 20px;
                        }
                        .success-title {
                            font-size: 36px;
                            color: #2c3e50;
                            margin-bottom: 15px;
                            font-weight: bold;
                        }
                        .success-desc {
                            font-size: 20px;
                            color: #7f8c8d;
                            margin-bottom: 30px;
                        }
                        .countdown {
                            font-size: 18px;
                            color: #e74c3c;
                            font-weight: 600;
                        }
                    </style>
                    <link rel="stylesheet" href="https://cdn.bootcdn.net/ajax/libs/font-awesome/6.4.0/css/all.min.css">
                </head>
                <body>
                    <div class="success-card">
                        <div class="icon"><i class="fas fa-check-circle"></i></div>
                        <h1 class="success-title">监控已开启</h1>
                        <p class="success-desc">操作已生效，设备状态已更新</p>
                        <p class="countdown">1秒后自动返回控制页面...</p>
                    </div>
                    <meta http-equiv="refresh" content="1;url=/">
                </body>
                </html>
            )";
        } else if (requestData.contains("GET /4")) {
            controlCameraById(7);// 关监控
            responseBody = R"(
                <!DOCTYPE html>
                <html lang="zh-CN">
                <head>
                    <meta charset="UTF-8">
                    <meta name="viewport" content="width=device-width, initial-scale=1.0">
                    <title>操作成功</title>
                    <style>
                        * { margin: 0; padding: 0; box-sizing: border-box; font-family: "Microsoft YaHei", Arial, sans-serif; }
                        body {
                            background-color: #f5f7fa;
                            display: flex;
                            flex-direction: column;
                            align-items: center;
                            justify-content: center;
                            min-height: 100vh;
                            padding: 20px;
                        }
                        .success-card {
                            background-color: white;
                            border-radius: 16px;
                            padding: 40px 30px;
                            box-shadow: 0 8px 24px rgba(0,0,0,0.1);
                            text-align: center;
                            max-width: 500px;
                            width: 100%;
                        }
                        .icon {
                            font-size: 80px;
                            color: #95a5a6;
                            margin-bottom: 20px;
                        }
                        .success-title {
                            font-size: 36px;
                            color: #2c3e50;
                            margin-bottom: 15px;
                            font-weight: bold;
                        }
                        .success-desc {
                            font-size: 20px;
                            color: #7f8c8d;
                            margin-bottom: 30px;
                        }
                        .countdown {
                            font-size: 18px;
                            color: #95a5a6;
                            font-weight: 600;
                        }
                    </style>
                    <link rel="stylesheet" href="https://cdn.bootcdn.net/ajax/libs/font-awesome/6.4.0/css/all.min.css">
                </head>
                <body>
                    <div class="success-card">
                        <div class="icon"><i class="fas fa-check-circle"></i></div>
                        <h1 class="success-title">监控已关闭</h1>
                        <p class="success-desc">操作已生效，设备状态已更新</p>
                        <p class="countdown">1秒后自动返回控制页面...</p>
                    </div>
                    <meta http-equiv="refresh" content="1;url=/">
                </body>
                </html>
            )";
        } else if (requestData.contains("GET /")) {
            // 根路径：返回带按钮的美观控制页面
            responseBody = R"(
                <!DOCTYPE html>
                <html lang="zh-CN">
                <head>
                    <meta charset="UTF-8">
                    <meta name="viewport" content="width=device-width, initial-scale=1.0">
                    <title>智能家居控制中心</title>
                    <style>
                        * { margin: 0; padding: 0; box-sizing: border-box; font-family: "Microsoft YaHei", Arial, sans-serif; }
                        body { background-color: #f5f7fa; padding: 20px; }
                        .container { max-width: 600px; margin: 0 auto; }
                        .title { text-align: center; color: #2c3e50; margin-bottom: 30px; font-size: 28px; font-weight: bold; }
                        .card { background-color: white; border-radius: 12px; padding: 25px; margin-bottom: 25px; box-shadow: 0 4px 12px rgba(0,0,0,0.08); }
                        .card-title { color: #34495e; margin-bottom: 20px; font-size: 20px; display: flex; align-items: center; }
                        .card-title i { margin-right: 10px; font-size: 24px; }
                        .btn-group { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }
                        .control-btn {
                            padding: 20px 15px;
                            border: none;
                            border-radius: 8px;
                            font-size: 18px;
                            font-weight: 600;
                            color: white;
                            cursor: pointer;
                            transition: all 0.3s ease;
                            display: flex;
                            align-items: center;
                            justify-content: center;
                            text-decoration: none;
                        }
                        .control-btn i { margin-right: 8px; font-size: 20px; }
                        .control-btn:hover { transform: translateY(-3px); box-shadow: 0 6px 15px rgba(0,0,0,0.1); }
                        .home-btn { background-color: #f39c12; }
                        .sleep-btn { background-color: #3498db; }
                        .camera-on-btn { background-color: #e74c3c; }
                        .camera-off-btn { background-color: #95a5a6; }
                        .footer { text-align: center; color: #7f8c8d; margin-top: 30px; font-size: 14px; }
                    </style>
                    <link rel="stylesheet" href="https://cdn.bootcdn.net/ajax/libs/font-awesome/6.4.0/css/all.min.css">
                </head>
                <body>
                    <div class="container">
                        <h1 class="title"><i class="fas fa-home"></i> 智能家居控制中心</h1>

                        <!-- 场景模式卡片 -->
                        <div class="card">
                            <h2 class="card-title"><i class="fas fa-layer-group"></i> 场景模式</h2>
                            <div class="btn-group">
                                <a href="/1" class="control-btn home-btn"><i class="fas fa-house-chimney"></i> 回家模式</a>
                                <a href="/2" class="control-btn sleep-btn"><i class="fas fa-moon"></i> 睡眠模式</a>
                            </div>
                        </div>

                        <!-- 设备控制卡片 -->
                        <div class="card">
                            <h2 class="card-title"><i class="fas fa-cogs"></i> 设备控制</h2>
                            <div class="btn-group">
                                <a href="/3" class="control-btn camera-on-btn"><i class="fas fa-video"></i> 开启监控</a>
                                <a href="/4" class="control-btn camera-off-btn"><i class="fas fa-video-slash"></i> 关闭监控</a>
                            </div>
                        </div>

                        <div class="footer">
                            <p>© 2026 智能家居监控平台 | 同一局域网内访问</p>
                        </div>
                    </div>
                </body>
                </html>
            )";
        } else {
            // 未知指令
            httpStatus = "404 Not Found";
            responseBody = R"(
                <!DOCTYPE html>
                <html lang="zh-CN">
                <head>
                    <meta charset="UTF-8">
                    <meta name="viewport" content="width=device-width, initial-scale=1.0">
                    <title>无效指令</title>
                    <style>
                        * { margin: 0; padding: 0; box-sizing: border-box; font-family: "Microsoft YaHei", Arial, sans-serif; }
                        body {
                            background-color: #f5f7fa;
                            display: flex;
                            flex-direction: column;
                            align-items: center;
                            justify-content: center;
                            min-height: 100vh;
                            padding: 20px;
                        }
                        .error-card {
                            background-color: white;
                            border-radius: 16px;
                            padding: 40px 30px;
                            box-shadow: 0 8px 24px rgba(0,0,0,0.1);
                            text-align: center;
                            max-width: 500px;
                            width: 100%;
                        }
                        .icon {
                            font-size: 80px;
                            color: #e74c3c;
                            margin-bottom: 20px;
                        }
                        .error-title {
                            font-size: 36px;
                            color: #2c3e50;
                            margin-bottom: 15px;
                            font-weight: bold;
                        }
                        .error-desc {
                            font-size: 20px;
                            color: #7f8c8d;
                            margin-bottom: 30px;
                        }
                        .back-btn {
                            display: inline-block;
                            padding: 12px 30px;
                            background-color: #3498db;
                            color: white;
                            font-size: 18px;
                            font-weight: 600;
                            text-decoration: none;
                            border-radius: 8px;
                            transition: all 0.3s ease;
                        }
                        .back-btn:hover {
                            transform: translateY(-3px);
                            box-shadow: 0 6px 15px rgba(0,0,0,0.1);
                        }
                    </style>
                    <link rel="stylesheet" href="https://cdn.bootcdn.net/ajax/libs/font-awesome/6.4.0/css/all.min.css">
                </head>
                <body>
                    <div class="error-card">
                        <div class="icon"><i class="fas fa-times-circle"></i></div>
                        <h1 class="error-title">无效指令</h1>
                        <p class="error-desc">请返回控制中心使用正确的功能按钮</p>
                        <a href="/" class="back-btn"><i class="fas fa-home"></i> 返回控制页面</a>
                    </div>
                </body>
                </html>
            )";
        }

        // 4. 构造并发送响应
        // 4. 构造并发送响应
        QByteArray response;
        response.append(QString("HTTP/1.1 %1\r\n").arg(httpStatus).toUtf8());
        response.append("Content-Type: text/html; charset=utf-8\r\n");
        response.append(QString("Content-Length: %1\r\n\r\n").arg(responseBody.toUtf8().length()).toUtf8());
        response.append(responseBody.toUtf8());

        httpSocket->write(response);
        httpSocket->flush();
        httpSocket->disconnectFromHost();
        httpSocket->deleteLater();
        timeoutTimer->deleteLater(); // 释放定时器
    });

    // ========== 原有：处理连接断开/错误 ==========
    connect(httpSocket, &QTcpSocket::disconnected, this, [=]() {
        timeoutTimer->stop();       // 断开连接时停止定时器
        timeoutTimer->deleteLater();// 释放定时器
        httpSocket->deleteLater();
    });
    connect(httpSocket, &QTcpSocket::errorOccurred, this, [=](QAbstractSocket::SocketError) {
        qDebug() << "HTTP 连接错误：" << httpSocket->errorString();
        timeoutTimer->stop();
        timeoutTimer->deleteLater();
        httpSocket->deleteLater();
    });

    // ========== 启动超时定时器 ==========
    timeoutTimer->start();
}

