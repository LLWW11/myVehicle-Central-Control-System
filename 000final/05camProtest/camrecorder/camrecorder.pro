QT += core gui qml quick quickcontrols2

CONFIG += c++14
CONFIG += thread
TEMPLATE = app
TARGET = camrecorderPro

CODECFORSRC = UTF-8

INCLUDEPATH += \
    src \
    src/camera \
    src/recorder \
    src/codec \
    src/rtsp

SOURCES += \
    main.cpp \
    src/cameracontroller.cpp \
    src/camera/cameracapture.cpp \
    src/camera/framestore.cpp \
    src/camera/previewimageprovider.cpp \
    src/recorder/avirecorderworker.cpp \
    src/recorder/aviwriter.cpp \
    src/codec/jpegencoder.cpp \
    src/codec/jpegframestore.cpp \
    src/jpegencoderworker.cpp \
    src/rtsp/rtspserverworker.cpp


HEADERS += \
    src/cameracontroller.h \
    src/camera/cameracapture.h \
    src/camera/framestore.h \
    src/camera/previewimageprovider.h \
    src/recorder/avirecorderworker.h \
    src/recorder/aviwriter.h \
    src/codec/jpegencoder.h \
    src/codec/jpegframestore.h \
    src/jpegencoderworker.h \
    src/rtsp/rtspserverworker.h


RESOURCES += qml.qrc

unix:!macx {
    LIBS += -lpthread

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

# GStreamer / RTSP-server 开发库：优先 pkg-config（交叉 SDK 已导出环境时），
# 否则退回与 jpeglib 相同的手工工具目录方式（需自行放置 ARM 版开发库）
unix:!macx {
    GST_PKG_OK = $$system(pkg-config --exists gstreamer-rtsp-server-1.0 && echo yes || echo no)
    equals(GST_PKG_OK, yes) {
        CONFIG += link_pkgconfig
        PKGCONFIG += \
            glib-2.0 \
            gstreamer-1.0 \
            gstreamer-app-1.0 \
            gstreamer-rtsp-server-1.0
    } else {
        GST_ROOT = /home/alientek/tools/gst
        exists($$GST_ROOT/lib/libgstrtspserver-1.0.so) {
            INCLUDEPATH += \
                $$GST_ROOT/include/glib-2.0 \
                $$GST_ROOT/lib/glib-2.0/include \
                $$GST_ROOT/include/gio-unix-2.0 \
                $$GST_ROOT/include/gstreamer-1.0 \
                $$GST_ROOT/include/gstreamer-app-1.0 \
                $$GST_ROOT/include/gstreamer-rtsp-server-1.0
            LIBS += -L$$GST_ROOT/lib \
                -lgstrtspserver-1.0 -lgstapp-1.0 -lgstreamer-1.0 \
                -lgobject-2.0 -lgmodule-2.0 -lglib-2.0
            QMAKE_RPATHDIR += $$GST_ROOT/lib
        } else {
            message(camrecorder: 未找到 GStreamer RTSP 开发库，请安装或放置到 $$GST_ROOT)
        }
    }
}

# 部署路径
target.path = /opt/ui/src/apps
!isEmpty(target.path): INSTALLS += target

# 接入综合桌面的远程对象客户端，并统一输出到 ui/src/apps。
SYSTEMUI_ROOT = $$clean_path($$PWD/../../..)
include($$SYSTEMUI_ROOT/client/client.pri)
INCLUDEPATH += $$SYSTEMUI_ROOT/client
