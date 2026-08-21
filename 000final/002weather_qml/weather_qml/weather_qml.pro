# Copyright (c) Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
# weather_qml.pro - Qt 天气预报(QML 版)

QT += core gui qml quick quickcontrols2 network

CONFIG += c++11
CONFIG += thread

TEMPLATE = app
TARGET = weather

CODECFORSRC = UTF-8

# 源文件
SOURCES += \
    main.cpp \
    weather.cpp

HEADERS += \
    weather.h

# 资源文件
RESOURCES += \
    qml.qrc \
    weather_icons.qrc

# 部署路径(正点原子板子)
target.path = /opt/ui/src/apps
!isEmpty(target.path): INSTALLS += target

# 警告deprecated API
DEFINES += QT_DEPRECATED_WARNINGS

# 接入综合桌面的远程对象客户端，并统一输出到 ui/src/apps。
SYSTEMUI_ROOT = $$clean_path($$PWD/../../..)
include($$SYSTEMUI_ROOT/client/client.pri)
INCLUDEPATH += $$SYSTEMUI_ROOT/client
