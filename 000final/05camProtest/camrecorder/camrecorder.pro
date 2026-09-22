QT += core gui qml quick quickcontrols2

CONFIG += c++14
CONFIG += thread


TEMPLATE = app
TARGET = camrecorderPro
DEFINES += QT_NO_KEYWORDS
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

    # pthread
    LIBS += -lpthread


    JPEG_ROOT = /home/alientek/tools/jpeglib

    exists($$JPEG_ROOT/include/jpeglib.h) {

        message(camrecorder: using custom JPEG library)

        INCLUDEPATH += $$JPEG_ROOT/include
        LIBS += -L$$JPEG_ROOT/lib -ljpeg

    } else {

        message(camrecorder: using system JPEG library)

        LIBS += -ljpeg
    }


    # --------------------------------------------------------
    # GStreamer / GstRTSPServer
    # --------------------------------------------------------

    GST_RTSP_OK = $$system(pkg-config --exists gstreamer-rtsp-server-1.0 && echo yes || echo no)

    equals(GST_RTSP_OK, yes) {

        CONFIG += link_pkgconfig

        PKGCONFIG += \
            gstreamer-1.0 \
            gstreamer-app-1.0 \
            gstreamer-rtsp-server-1.0

        message(camrecorder: GStreamer RTSP Server found)

    } else {

        error(camrecorder: gstreamer-rtsp-server-1.0 not found)
    }
}


target.path = /opt/ui/src/apps
!isEmpty(target.path): INSTALLS += target


SYSTEMUI_ROOT = $$clean_path($$PWD/../../..)
include($$SYSTEMUI_ROOT/client/client.pri)
INCLUDEPATH += $$SYSTEMUI_ROOT/client

CONFIG -= release
CONFIG -= debug_and_release
CONFIG += debug
CONFIG -= strip

QMAKE_CFLAGS_DEBUG += -g -O0
QMAKE_CXXFLAGS_DEBUG += -g -O0

QMAKE_LFLAGS -= -s
QMAKE_LFLAGS -= -Wl,-s