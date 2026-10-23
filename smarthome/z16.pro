QT += core gui sql multimedia printsupport
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
QT += core gui network
CONFIG += c++17

# 添加编译标志解决 qcustomplot 编译问题
QMAKE_CXXFLAGS += -Wa,-mbig-obj
win32: QMAKE_CXXFLAGS += -fno-keep-inline-dllexport

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    db_extension.cpp \
    record_window.cpp \
    qcustomplot.cpp

HEADERS += \
    mainwindow.h \
    devicecontroller.h \
    db_extension.h \
    record_window.h \
    qcustomplot.h

TARGET = SmartHome
TEMPLATE = app
TARGET = TcpServerDemo
RESOURCES += resources.qrc
