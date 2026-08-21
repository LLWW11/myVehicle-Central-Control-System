QT += core gui qml quick quickcontrols2

CONFIG += c++14
CONFIG += thread
TEMPLATE = app
TARGET = camrecorder

CODECFORSRC = UTF-8

INCLUDEPATH += \
    src \
    src/camera \
    src/recorder

SOURCES += \
    main.cpp \
    src/cameracontroller.cpp \
    src/camera/cameracapture.cpp \
    src/camera/framestore.cpp \
    src/camera/previewimageprovider.cpp \
    src/recorder/avirecorderworker.cpp \
    src/recorder/aviwriter.cpp

HEADERS += \
    src/cameracontroller.h \
    src/camera/cameracapture.h \
    src/camera/framestore.h \
    src/camera/previewimageprovider.h \
    src/recorder/avirecorderworker.h \
    src/recorder/aviwriter.h

RESOURCES += qml.qrc

unix:!macx {
    LIBS += -lpthread

    # 优先使用原 camMulti 已验证的 libjpeg 安装目录。
    JPEG_ROOT = /home/alientek/tools/jpeglib
    exists($$JPEG_ROOT/include/jpeglib.h) {
        INCLUDEPATH += $$JPEG_ROOT/include
        LIBS += -L$$JPEG_ROOT/lib -ljpeg
        QMAKE_RPATHDIR += $$JPEG_ROOT/lib
    } else {
        # 如果 SDK sysroot 已提供 libjpeg，则直接使用工具链中的库。
        LIBS += -ljpeg
    }
}

# 部署路径(正点原子板子)
target.path = /opt/ui/src/apps
!isEmpty(target.path): INSTALLS += target

# 接入综合桌面的远程对象客户端，并统一输出到 ui/src/apps。
SYSTEMUI_ROOT = $$clean_path($$PWD/../../..)
include($$SYSTEMUI_ROOT/client/client.pri)
INCLUDEPATH += $$SYSTEMUI_ROOT/client
