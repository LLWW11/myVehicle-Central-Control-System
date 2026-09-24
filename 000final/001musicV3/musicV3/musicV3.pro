QT += core gui quick network
CONFIG += c++11
DEFINES += QT_DEPRECATED_WARNINGS

TEMPLATE = app
TARGET = musicV3
CODECFORSRC = UTF-8
INCLUDEPATH += .

SOURCES += \
    main.cpp \
    albumimage.cpp \
    MusicCache.cpp \
    MusicEngine.cpp \
    MusicListModel.cpp \
    MusicLyricModel.cpp \
    audio/Mpg123Runtime.cpp \
    audio/Mpg123Decoder.cpp \
    audio/AlsaPcmOutput.cpp \
    audio/AudioPlaybackThread.cpp \
    player/AlsaPlayer.cpp \
    network/MusicApi.cpp \
    network/Downloader.cpp

HEADERS += \
    albumimage.h \
    data/Song.h \
    data/SongData.h \
    lyrics/LyricParser.h \
    MusicCache.h \
    MusicEngine.h \
    MusicListModel.h \
    MusicLyricModel.h \
    audio/AudioFormat.h \
    audio/Mpg123Runtime.h \
    audio/Mpg123Decoder.h \
    audio/AlsaPcmOutput.h \
    audio/AudioPlaybackThread.h \
    player/AlsaPlayer.h \
    player/PlayerState.h \
    network/MusicApi.h \
    network/Downloader.h

RESOURCES += qml.qrc res.qrc

# 可由交叉编译命令覆盖 libmpg123 的安装前缀。
isEmpty(MPGLIB_DIR): MPGLIB_DIR = /home/alientek/tools/mpglib
INCLUDEPATH += $$MPGLIB_DIR/include
LIBS += -L$$MPGLIB_DIR/lib -lmpg123 -lasound

unix:!android {
    target.path = /opt/ui/src/apps
    INSTALLS += target
}
