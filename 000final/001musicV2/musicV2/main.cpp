#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTextCodec>
#include "systemuicommonapiclient.h"
#include <QQmlEngine>
#include "albumimage.h"
#include "MusicEngine.h"
#include "MusicListModel.h"
#include "MusicLyricModel.h"

/**
 * @brief 创建音乐播放器应用、注册 QML 类型并加载主界面。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return 应用事件循环的退出码。
 */
int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    QGuiApplication app(argc, argv);
    qmlRegisterType<SystemUICommonApiClient>("com.alientek.qmlcomponents", 1, 0, "SystemUICommonApiClient");
    QQmlApplicationEngine engine;
    qmlRegisterType<MusicLyricModel>("com.alientek.qmlcomponents", 1, 0, "LyricModel");
    qmlRegisterType<MusicListModel>("com.alientek.qmlcomponents", 1, 0, "PlayListModel");
    qmlRegisterType<MusicEngine>("com.alientek.qmlcomponents", 1, 0, "MusicEngine");
    qmlRegisterType<AlbumImage>("com.alientek.qmlcomponents", 1, 0, "AlbumImage");

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);
    engine.load(url);
    return app.exec();
}
