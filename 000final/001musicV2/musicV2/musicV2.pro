QT += core gui quick network multimedia

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Refer to the documentation for the
# deprecated API to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

TARGET = musicV2
TEMPLATE = app
CODECFORSRC = UTF-8

INCLUDEPATH += .

SOURCES += \
        albumimage.cpp \
        main.cpp \
        MusicCache.cpp \
        MusicEngine.cpp \
        MusicListModel.cpp \
        MusicLyricModel.cpp \
        network/Downloader.cpp \
        network/MusicApi.cpp \
        player/AlsaPlayer.cpp

HEADERS += \
        albumimage.h \
        data/Song.h \
        data/SongData.h \
        lyrics/LyricParser.h \
        MusicCache.h \
        MusicEngine.h \
        MusicListModel.h \
        MusicLyricModel.h \
        network/Downloader.h \
        network/MusicApi.h \
        player/AlsaPlayer.h \
        player/PlayerState.h

RESOURCES += qml.qrc \
    res.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# ---------- 纯 C ALSA + mpg123 播放引擎 ----------
SOURCES += \
    audio/alsa_engine.c \
    audio/alsa_player.c \
    audio/mp3_decoder.c

HEADERS += \
    audio/alsa_engine.h \
    audio/alsa_player.h \
    audio/mp3_decoder.h

DEFINES += USE_ALSA_BACKEND

MPGLIB_DIR = /home/alientek/tools/mpglib
LIBS += -L$$MPGLIB_DIR/lib -lasound -lmpg123 -lpthread
INCLUDEPATH += $$MPGLIB_DIR/include
QMAKE_CFLAGS += -std=gnu11

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/ui/src/apps
!isEmpty(target.path): INSTALLS += target

# 接入综合桌面的远程对象客户端，并统一输出到 ui/src/apps。
SYSTEMUI_ROOT = $$clean_path($$PWD/../../..)
include($$SYSTEMUI_ROOT/client/client.pri)
INCLUDEPATH += $$SYSTEMUI_ROOT/client
