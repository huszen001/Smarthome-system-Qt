#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
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
#include <QList>
#include <QMenu>
#include <QDialog>
#include <QCheckBox>
#include <QListWidget>
#include <QTimer>
#include <QDateTime>
#include <QSet>
#include <QEvent>
#include <QMouseEvent>
#include <QScrollArea>
// 系统管理模块所需头文件
#include <QSettings>
#include <QFileDialog>
#include <QComboBox>
#include <QTableWidget>
#include <QFile>
#include <QSoundEffect>
#include <QUrl>
#include <QDebug>
#include <QtHttpServer/QHttpServer>

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress> // 网络地址类
class DeviceController;  // 前向声明

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    // 登录相关
    void doLogin();
    void showRegister();
    void showReset();
    void backToLogin();
    void doRegister();
    void doReset();
    void doLogout();

    // 处理新客户端连接
    void onNewConnection();
    // 处理客户端数据
    void onReadyRead();
    // 处理客户端断开
    void onDisconnected();
    // 给手机发送字符串
    void sendToPhone(const QString &msg);

    // ========== 新增 HTTP 相关槽函数 ==========
    void startHttpServer();       // 启动 HTTP 服务
    void onHttpNewConnection();   // 处理 HTTP 新连接



    // 首页设备控制（改为控制客厅设备）
    void controlLivingRoomLight();
    void controlLivingRoomAircon();
    void sceneHome();
    void sceneSleep();

    // 设备控制页（新：分类页和列表页）
    void showFurnitureCategory();
    void showFurnitureList(const QString& category);
    void backToCategory();
    void backToHome();

    // 家具控制函数
    void controlLightById(int id);
    void controlAirconById(int id);
    void controlCurtainById(int id);
    void controlCameraById(int id);
    void brightnessUpById(int id);
    void brightnessDownById(int id);
    void tempUpById(int id);
    void tempDownById(int id);
    void opennessUpById(int id);
    void opennessDownById(int id);
    void cameraLeftById(int id);
    void cameraRightById(int id);
    void cameraAngleChangedById(int id, int angle);

    // 报警相关
    void showAlarm();
    void checkAlarm();
    void addAlarmRecord(QString type, QString content, QString alarmType = "");
    void clearAlarm();
    void updateEnvData();

    // 设备统计
    void updateDeviceStats();
    void showOnlineDevices();
    void showOfflineDevices();
    void showErrorDevices();
    void checkForErrors();
    void highlightChangedLabels();

    // 场景模式相关
    void loadSceneModes();
    void saveSceneModes();
    void showCustomModeMenu();
    void addNewScene();
    void editScene(const QString &name);
    void deleteScene(const QString &name);
    void updateCustomButtons();

    // 系统管理模块槽函数
    void loadDeviceList();
    void backupDatabase();
    void restoreDatabase();
    void saveSysSettings();
    void loadSysSettings();
    void testDeviceConn();
    void updateSoundFile();
    void playAlarmSound(bool isLoop);
    void setDeviceStatus(int statusCode, const QString &statusText);
    void searchDevice(const QString &keyword);
    void applyTheme(int themeIndex);

private:
    // ========== 场景模式结构体 ==========
    // 在 mainwindow.h 中修改 SceneMode 结构体：
    struct SceneMode {
        QString name;
        QMap<int, bool> deviceStatus;  // 设备ID -> 开关状态
        QMap<int, int> deviceValues;    // 设备ID -> 参数值（亮度、温度等）

        // 为了兼容旧代码，保留这些字段但不使用
        bool lightOn;
        int lightBright;
        bool airconOn;
        int airconTemp;
        bool curtainOn;
        int curtainOpenness;
        bool cameraOn;
        int cameraAngle;

        SceneMode() {
            lightOn = false;
            lightBright = 50;
            airconOn = false;
            airconTemp = 24;
            curtainOn = false;
            curtainOpenness = 50;
            cameraOn = false;
            cameraAngle = 0;
        }
    };

    // 家具数据结构
    struct Furniture {
        int id;
        QString type;
        QString name;
        QString room;
        int status;
        int brightness;
        int temperature;
        int angle;
        int openness;
        int fault;
    };

    QTcpServer *tcpServer;  // 声明 TCP 服务器指针
    QLabel *statusLabel;    // 声明 Label 指针（替代 ui->label）
    QTcpSocket *clientSocket; // 客户端连接
    QTcpServer *httpServer;  // HTTP 服务器对象

    // 基础UI组件
    QStackedWidget *stackedWidget;

    // ========== 登录页 ==========
    QWidget *loginPage;
    QLineEdit *loginUsername;
    QLineEdit *loginPassword;
    QPushButton *loginBtn;
    QPushButton *registerBtn;
    QPushButton *resetBtn;

    // ========== 注册页 ==========
    QWidget *registerPage;
    QLineEdit *regUsername;
    QLineEdit *regPassword;
    QLineEdit *regConfirmPassword;
    QPushButton *regConfirmBtn;
    QPushButton *regBackBtn;

    // ========== 重置页 ==========
    QWidget *resetPage;
    QLineEdit *resetUsername;
    QLineEdit *resetNewPassword;
    QLineEdit *resetConfirmPassword;
    QPushButton *resetConfirmBtn;
    QPushButton *resetBackBtn;

    // ========== 首页 ==========
    QWidget *homePage;
    QLabel *welcomeLabel;
    QPushButton *livingRoomLightBtn;      // 客厅灯按钮
    QPushButton *livingRoomAirconBtn;     // 客厅空调按钮
    QPushButton *homeModeBtn;
    QPushButton *sleepModeBtn;
    QPushButton *logoutBtn;
    QLabel *livingRoomLightStatusLabel;   // 客厅灯状态
    QLabel *livingRoomLightBrightnessLabel;
    QLabel *livingRoomAirconStatusLabel;
    QLabel *livingRoomAirconTempLabel;
    QPushButton *alarmEntryBtn;

    // 设备故障状态（保留，但改为基于数据库）
    bool lightFault;
    bool airconFault;
    QTimer *lightFixTimer;
    QTimer *airconFixTimer;
    QString faultyDevice;

    // 自定义模式按钮
    QPushButton *custom1Btn;
    QPushButton *custom2Btn;
    QPushButton *customModeBtn;

    // 设备统计标签
    QLabel *m_onlineLabel;
    QLabel *m_offlineLabel;
    QLabel *m_errorLabel;
    int m_onlineDevices;
    int m_offlineDevices;
    int m_errorDevices;
    QTimer *m_statsTimer;

    // ========== 家具分类页 ==========
    QWidget *furnitureCategoryPage;
    QPushButton *lightCategoryBtn;
    QPushButton *airconCategoryBtn;
    QPushButton *cameraCategoryBtn;
    QPushButton *curtainCategoryBtn;
    QPushButton *backFromCategoryBtn;

    // ========== 家具列表页 ==========
    QWidget *furnitureListPage;
    QString m_currentCategory;
    QVBoxLayout *furnitureListLayout;
    QPushButton *backFromListBtn;
    QList<QWidget*> furnitureItemWidgets;  // 存储每个家具的控件组
    QScrollArea *furnitureScrollArea;  // 新增

    // ========== 报警页 ==========
    QWidget *alarmPage;
    QLabel *tempValue, *humiValue, *lightValue;
    QSlider *tempSlider, *humiSlider, *lightSlider;
    QPushButton *alarmBackBtn, *clearAlarmBtn;
    QListWidget *alarmList;
    QTimer *alarmCheckTimer;

    // 报警阈值（双阈值）
    int tempMin, tempMax;
    int humiMin, humiMax;
    int lightMin, lightMax;
    bool alarmEnabled;
    int deviceCheckCounter;
    QSet<QString> ignoredAlarms;

    // ========== 设备控制器（保留但主要改用数据库） ==========
    DeviceController *m_deviceCtrl;

    // ========== 家具数据 ==========
    QList<Furniture> m_furniture;
    void loadFurnitureFromDb();
    void updateFurnitureInDb(const Furniture& f);
    Furniture* getFurnitureById(int id);
    QList<Furniture> getFurnitureByType(const QString& type);
    Furniture* getLivingRoomLight();      // 获取客厅的一盏灯（用于场景模式）
    Furniture* getLivingRoomAircon();      // 获取客厅空调

    // ========== 场景模式列表 ==========
    QList<SceneMode> sceneModes;

    // ========== 系统管理页 ==========
    QWidget *sysManagePage;

    // 设备管理控件
    QLineEdit *devAddName;
    QComboBox *devAddGroup, *devAddType;
    QTableWidget *devTable;
    QPushButton *devAddBtn, *devDelBtn, *devTestBtn;

    // 系统设置控件
    QComboBox *themeCombo;
    QSpinBox *refreshSpin;
    QCheckBox *soundCheck;
    QPushButton *backupBtn, *restoreBtn;

    // 音效相关
    QSoundEffect *alarmSound;
    QComboBox *soundCombo;
    QPushButton *testSoundBtn;

    // 当前登录用户
    QString m_currentUser;
    QTimer* m_envTimer;

    // ========== 初始化函数 ==========
    void initDatabase();
    void createLoginPage();
    void createRegisterPage();
    void createResetPage();
    void createHomePage();
    void createFurnitureCategoryPage();   // 新：分类页
    void createFurnitureListPage();       // 新：列表页
    void createAlarmPage();
    void createSysManagePage();

    // ===== 场景模式相关方法 =====
    void applySceneMode(const SceneMode &scene);

    // 数据库辅助函数
    static QSqlDatabase getDbConnection();
    static QString encryptPassword(QString pwd);

    void refreshDevicePage();
};

#endif
