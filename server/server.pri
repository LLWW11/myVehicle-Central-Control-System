QT += remoteobjects

SOURCES += \
    $$PWD/../systemui/launchintent.cpp \
    $$PWD/systemuicommonapiserver.cpp

HEADERS += \
    $$PWD/../systemui/launchintent.h \
    $$PWD/systemuicommonapiserver.h

REPC_SOURCE += \
    ../Repcs/systemuicommonapi.rep

unix {
    SRC_FILE = $$OUT_PWD/$$TARGET
    DST_FILE = $$OUT_PWD/../ui/
    QMAKE_POST_LINK += $(STRIP) $$SRC_FILE; cp $$SRC_FILE $$DST_FILE; \
}

RESOURCES += \
    $$PWD/common.qrc

