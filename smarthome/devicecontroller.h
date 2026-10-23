#ifndef DEVICECONTROLLER_H
#define DEVICECONTROLLER_H

#include <QObject>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QDebug>
#include <QLayout>
#include <QGroupBox>
#include <QList>
#include <QHBoxLayout>
#include <QSlider>
#include <QSpinBox>

// 设备控制器类 - 简化版本，主要兼容原有代码
class DeviceController : public QObject
{
    Q_OBJECT

public:
    bool isLightOn() const { return lightOn; }
    int getLightBrightness() const { return lightBrightness; }
    bool isAirconOn() const { return airconOn; }
    int getAirconTemp() const { return airconTemp; }
    bool isCurtainOn() const { return curtainOn; }
    int getCurtainOpenness() const { return curtainOpenness; }
    bool isCameraOn() const { return cameraOn; }
    int getCameraAngle() const { return cameraAngle; }
    void setCameraAngle(int angle) {
        if (cameraOn && angle >= -90 && angle <= 90) {
            cameraAngle = angle;
            updateCameraAngle();
        }
    }

    void setCurtainOpenness(int openness) {
        if (curtainOn && openness >= 0 && openness <= 100) {
            curtainOpenness = openness;
            updateCurtainUI();
        }
    }

    explicit DeviceController(QObject *parent = nullptr) : QObject(parent)
    {
        lightOn = false;
        lightBrightness = 80;
        airconOn = false;
        airconTemp = 26;
        curtainOn = false;
        curtainOpenness = 50;
        cameraOn = false;
        cameraAngle = 0;
    }

    // 设置灯光UI元素（保留兼容）
    void setLightUI(QLabel* status, QPushButton* btn, QLabel* brightLabel,
                    QPushButton* up, QPushButton* down) {
        m_lightStatus = status;
        m_lightBtn = btn;
        m_brightLabel = brightLabel;
        m_brightUp = up;
        m_brightDown = down;

        updateLightUI();
    }

    // 设置空调UI元素
    void setAirconUI(QLabel* status, QPushButton* btn, QLabel* tempLabel,
                     QPushButton* up, QPushButton* down) {
        m_airconStatus = status;
        m_airconBtn = btn;
        m_tempLabel = tempLabel;
        m_tempUp = up;
        m_tempDown = down;

        updateAirconUI();
    }

    // 设置窗帘UI元素
    void setCurtainUI(QLabel* status, QPushButton* btn, QLabel* opennessLabel = nullptr,
                      QPushButton* up = nullptr, QPushButton* down = nullptr) {
        m_curtainStatus = status;
        m_curtainBtn = btn;
        m_curtainOpennessLabel = opennessLabel;
        m_curtainUp = up;
        m_curtainDown = down;

        updateCurtainUI();
    }

    // 设置摄像头UI元素
    void setCameraUI(QLabel* status, QPushButton* btn,
                     QLabel* angleLabel = nullptr,
                     QSlider* slider = nullptr,
                     QPushButton* leftBtn = nullptr,
                     QPushButton* rightBtn = nullptr,
                     QSpinBox* spinBox = nullptr) {
        m_cameraStatus = status;
        m_cameraBtn = btn;
        m_cameraAngleLabel = angleLabel;
        m_cameraAngleSlider = slider;
        m_cameraLeftBtn = leftBtn;
        m_cameraRightBtn = rightBtn;
        m_cameraAngleSpin = spinBox;

        if (m_cameraAngleSlider) {
            m_cameraAngleSlider->setRange(-90, 90);
            m_cameraAngleSlider->setValue(0);
            m_cameraAngleSlider->setEnabled(false);
        }

        if (m_cameraAngleLabel) {
            m_cameraAngleLabel->setText("角度: 0°");
        }

        if (m_cameraLeftBtn) {
            m_cameraLeftBtn->setEnabled(false);
            m_cameraLeftBtn->setText("←");
        }
        if (m_cameraRightBtn) {
            m_cameraRightBtn->setEnabled(false);
            m_cameraRightBtn->setText("→");
        }

        if (m_cameraAngleSpin) {
            m_cameraAngleSpin->setRange(-90, 90);
            m_cameraAngleSpin->setValue(0);
            m_cameraAngleSpin->setEnabled(false);
            m_cameraAngleSpin->setSuffix("°");
        }

        updateCameraUI();
    }

    // 公共方法：从外部设置灯光状态
    void setLightState(bool on, int brightness) {
        lightOn = on;
        if (brightness >= 0 && brightness <= 100) {
            lightBrightness = brightness;
        }
        updateLightUI();
    }

    // 公共方法：从外部设置空调状态
    void setAirconState(bool on, int temp) {
        airconOn = on;
        if (temp >= 16 && temp <= 30) {
            airconTemp = temp;
        }
        updateAirconUI();
    }

    // 灯光按钮点击
    void onLightClicked() {
        lightOn = !lightOn;
        updateLightUI();
        QMessageBox::information(nullptr, "提示", QString("灯光已%1").arg(lightOn ? "开启" : "关闭"));
    }

    // 亮度增加
    void onBrightnessUp() {
        if (lightOn && lightBrightness < 100) {
            lightBrightness = qMin(100, lightBrightness + 10);
            updateLightUI();
        }
    }

    // 亮度减少
    void onBrightnessDown() {
        if (lightOn && lightBrightness > 0) {
            lightBrightness = qMax(0, lightBrightness - 10);
            updateLightUI();
        }
    }

    // 空调按钮点击
    void onAirconClicked() {
        airconOn = !airconOn;
        updateAirconUI();
        QMessageBox::information(nullptr, "提示", QString("空调已%1").arg(airconOn ? "开启" : "关闭"));
    }

    // 温度增加
    void onTempUp() {
        if (airconOn && airconTemp < 30) {
            airconTemp++;
            updateAirconUI();
        }
    }

    // 温度减少
    void onTempDown() {
        if (airconOn && airconTemp > 16) {
            airconTemp--;
            updateAirconUI();
        }
    }

    // 窗帘按钮点击
    void onCurtainClicked() {
        curtainOn = !curtainOn;
        updateCurtainUI();
        QMessageBox::information(nullptr, "提示", QString("窗帘已%1").arg(curtainOn ? "开启" : "关闭"));
    }

    // 窗帘开度增加
    void onCurtainOpennessUp() {
        if (curtainOn && curtainOpenness < 100) {
            curtainOpenness = qMin(100, curtainOpenness + 10);
            updateCurtainUI();
        }
    }

    // 窗帘开度减少
    void onCurtainOpennessDown() {
        if (curtainOn && curtainOpenness > 0) {
            curtainOpenness = qMax(0, curtainOpenness - 10);
            updateCurtainUI();
        }
    }

    // 摄像头按钮点击
    void onCameraClicked() {
        cameraOn = !cameraOn;
        updateCameraUI();
        QMessageBox::information(nullptr, "提示", QString("摄像头已%1").arg(cameraOn ? "开启" : "关闭"));
    }

    // 摄像头左转
    void onCameraLeft() {
        if (cameraOn && cameraAngle > -90) {
            cameraAngle = qMax(-90, cameraAngle - 15);
            updateCameraAngle();
        }
    }

    // 摄像头右转
    void onCameraRight() {
        if (cameraOn && cameraAngle < 90) {
            cameraAngle = qMin(90, cameraAngle + 15);
            updateCameraAngle();
        }
    }

    // 滑块角度改变
    void onCameraAngleChanged(int angle) {
        if (cameraOn) {
            cameraAngle = angle;
            updateCameraAngle();
        }
    }

    // 数字框角度改变
    void onCameraAngleSpinChanged(int angle) {
        if (cameraOn) {
            cameraAngle = angle;
            updateCameraAngle();
        }
    }

signals:
    void lightStateChanged(bool on, int brightness);
    void airconStateChanged(bool on, int temp);

private:
    void updateLightUI() {
        if (!m_lightStatus || !m_lightBtn || !m_brightLabel || !m_brightUp || !m_brightDown)
            return;

        if (lightOn) {
            m_lightBtn->setText("关闭灯光");
            m_lightStatus->setText(QString("状态: 开启 (亮度: %1%)").arg(lightBrightness));
            m_brightLabel->setText(QString("亮度: %1%").arg(lightBrightness));
            m_brightUp->setEnabled(true);
            m_brightDown->setEnabled(true);
        } else {
            m_lightBtn->setText("开启灯光");
            m_lightStatus->setText("状态: 关闭");
            m_brightLabel->setText(QString("亮度: %1%").arg(lightBrightness));
            m_brightUp->setEnabled(false);
            m_brightDown->setEnabled(false);
        }

        emit lightStateChanged(lightOn, lightBrightness);
    }

    void updateAirconUI() {
        if (!m_airconStatus || !m_airconBtn || !m_tempLabel || !m_tempUp || !m_tempDown)
            return;

        if (airconOn) {
            m_airconBtn->setText("关闭空调");
            m_airconStatus->setText(QString("状态: 开启 (温度: %1°C)").arg(airconTemp));
            m_tempLabel->setText(QString("温度: %1°C").arg(airconTemp));
            m_tempUp->setEnabled(true);
            m_tempDown->setEnabled(true);
        } else {
            m_airconBtn->setText("开启空调");
            m_airconStatus->setText("状态: 关闭");
            m_tempLabel->setText(QString("温度: %1°C").arg(airconTemp));
            m_tempUp->setEnabled(false);
            m_tempDown->setEnabled(false);
        }

        emit airconStateChanged(airconOn, airconTemp);
    }

    void updateCurtainUI() {
        if (!m_curtainStatus || !m_curtainBtn)
            return;

        if (curtainOn) {
            m_curtainBtn->setText("关闭窗帘");
            m_curtainStatus->setText(QString("状态: 开启 (开度: %1%)").arg(curtainOpenness));

            if (m_curtainOpennessLabel) {
                m_curtainOpennessLabel->setText(QString("开度: %1%").arg(curtainOpenness));
            }
            if (m_curtainUp && m_curtainDown) {
                m_curtainUp->setEnabled(true);
                m_curtainDown->setEnabled(true);
            }
        } else {
            m_curtainBtn->setText("开启窗帘");
            m_curtainStatus->setText("状态: 关闭");

            if (m_curtainOpennessLabel) {
                m_curtainOpennessLabel->setText(QString("开度: %1%").arg(curtainOpenness));
            }
            if (m_curtainUp && m_curtainDown) {
                m_curtainUp->setEnabled(false);
                m_curtainDown->setEnabled(false);
            }
        }
    }

    void updateCameraUI() {
        if (!m_cameraStatus || !m_cameraBtn)
            return;

        if (cameraOn) {
            m_cameraBtn->setText("关闭摄像头");
            m_cameraStatus->setText(QString("状态: 开启 (角度: %1°)").arg(cameraAngle));

            if (m_cameraAngleSlider) m_cameraAngleSlider->setEnabled(true);
            if (m_cameraLeftBtn) m_cameraLeftBtn->setEnabled(true);
            if (m_cameraRightBtn) m_cameraRightBtn->setEnabled(true);
            if (m_cameraAngleSpin) m_cameraAngleSpin->setEnabled(true);
            if (m_cameraAngleLabel) m_cameraAngleLabel->setEnabled(true);
        } else {
            m_cameraBtn->setText("开启摄像头");
            m_cameraStatus->setText("状态: 关闭");

            if (m_cameraAngleSlider) m_cameraAngleSlider->setEnabled(false);
            if (m_cameraLeftBtn) m_cameraLeftBtn->setEnabled(false);
            if (m_cameraRightBtn) m_cameraRightBtn->setEnabled(false);
            if (m_cameraAngleSpin) m_cameraAngleSpin->setEnabled(false);
        }

        updateCameraAngle();
    }

    void updateCameraAngle() {
        if (!cameraOn) return;

        QString angleText = QString("角度: %1°").arg(cameraAngle);

        if (m_cameraAngleLabel) {
            m_cameraAngleLabel->setText(angleText);
        }

        if (m_cameraStatus) {
            m_cameraStatus->setText(QString("状态: 开启 (角度: %1°)").arg(cameraAngle));
        }

        if (m_cameraAngleSlider) {
            m_cameraAngleSlider->setValue(cameraAngle);
        }

        if (m_cameraAngleSpin) {
            m_cameraAngleSpin->setValue(cameraAngle);
        }

        if (m_cameraLeftBtn) {
            m_cameraLeftBtn->setEnabled(cameraOn && cameraAngle > -90);
        }
        if (m_cameraRightBtn) {
            m_cameraRightBtn->setEnabled(cameraOn && cameraAngle < 90);
        }
    }

    // UI元素指针
    QLabel *m_lightStatus = nullptr;
    QPushButton *m_lightBtn = nullptr;
    QLabel *m_brightLabel = nullptr;
    QPushButton *m_brightUp = nullptr;
    QPushButton *m_brightDown = nullptr;

    QLabel *m_airconStatus = nullptr;
    QPushButton *m_airconBtn = nullptr;
    QLabel *m_tempLabel = nullptr;
    QPushButton *m_tempUp = nullptr;
    QPushButton *m_tempDown = nullptr;

    QLabel *m_curtainStatus = nullptr;
    QPushButton *m_curtainBtn = nullptr;
    QLabel *m_curtainOpennessLabel = nullptr;
    QPushButton *m_curtainUp = nullptr;
    QPushButton *m_curtainDown = nullptr;

    QLabel *m_cameraStatus = nullptr;
    QPushButton *m_cameraBtn = nullptr;
    QLabel *m_cameraAngleLabel = nullptr;
    QSlider *m_cameraAngleSlider = nullptr;
    QPushButton *m_cameraLeftBtn = nullptr;
    QPushButton *m_cameraRightBtn = nullptr;
    QSpinBox *m_cameraAngleSpin = nullptr;

    // 设备状态
    bool lightOn;
    int lightBrightness;
    bool airconOn;
    int airconTemp;
    bool curtainOn;
    int curtainOpenness;
    bool cameraOn;
    int cameraAngle;
};

#endif // DEVICECONTROLLER_H
