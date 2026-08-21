TARGET = maplite
TEMPLATE = app

# Build the embedded target as a release application by default.
CONFIG += c++11 release
CONFIG -= debug

QT += core gui qml quick network positioning location serialport

SOURCES += \
    main.cpp \
    seriallocation.cpp
HEADERS += \
    seriallocation.h
RESOURCES += \
    mapviewer.qrc

OTHER_FILES += \
    mapviewer.qml \
    README.md

target.path = /opt/ui/src/apps
INSTALLS += target

# 接入综合桌面的远程对象客户端，并统一输出到 ui/src/apps。
SYSTEMUI_ROOT = $$clean_path($$PWD/../../..)
include($$SYSTEMUI_ROOT/client/client.pri)
INCLUDEPATH += $$SYSTEMUI_ROOT/client
