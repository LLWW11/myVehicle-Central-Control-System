QT += core testlib
CONFIG += testcase c++11
TEMPLATE = app
TARGET = tst_audio
INCLUDEPATH += ..

SOURCES += \
    tst_audio.cpp \
    ../MusicCache.cpp \
    ../MusicListModel.cpp

HEADERS += \
    ../audio/AudioFormat.h \
    ../MusicCache.h \
    ../MusicListModel.h \
    ../data/Song.h \
    ../data/SongData.h
