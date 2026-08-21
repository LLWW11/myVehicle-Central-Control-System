QT += remoteobjects

# 嵌套工程显式传入根目录；原版一级应用则自动回退到上级目录。
isEmpty(SYSTEMUI_ROOT) {
    SYSTEMUI_ROOT = $$clean_path($$_PRO_FILE_PWD_/..)
}

SOURCES += \
    $$SYSTEMUI_ROOT/client/systemuicommonapiclient.cpp

REPC_REPLICA += \
    $$SYSTEMUI_ROOT/Repcs/systemuicommonapi.rep

HEADERS += \
    $$SYSTEMUI_ROOT/client/systemuicommonapiclient.h

RESOURCES += $$SYSTEMUI_ROOT/client/common.qrc

# 交叉编译成功后，将应用统一复制到 SystemUI 的部署目录。
unix {
    SRC_FILE = $$OUT_PWD/$$TARGET
    DST_FILE = $$SYSTEMUI_ROOT/ui/src/apps
    QMAKE_POST_LINK += $(STRIP) $$SRC_FILE; cp $$SRC_FILE $$DST_FILE; \
}
