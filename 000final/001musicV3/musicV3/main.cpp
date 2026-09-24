#include "audio/Mpg123Runtime.h"
#include "MusicEngine.h"
#include "MusicListModel.h"
#include "MusicLyricModel.h"
#include "albumimage.h"
#include "player/PlayerState.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QDebug>

// 初始化 Qt、全局 mpg123、QML 类型并加载独立播放器窗口。
int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("musicV3"));
    app.setOrganizationName(QStringLiteral("Ceru"));

    Mpg123Runtime runtime;
    if (!runtime.isValid())
    {
        qCritical() << "libmpg123 初始化失败:" << runtime.errorString();
        return 1;
    }

    qRegisterMetaType<PlayerState>("PlayerState");
    qRegisterMetaType<PlaybackFinishReason>("PlaybackFinishReason");
    qRegisterMetaType<PlaybackError>("PlaybackError");
    qmlRegisterType<MusicLyricModel>("com.alientek.qmlcomponents", 1, 0, "LyricModel");
    qmlRegisterType<MusicListModel>("com.alientek.qmlcomponents", 1, 0, "PlayListModel");
    qmlRegisterType<MusicEngine>("com.alientek.qmlcomponents", 1, 0, "MusicEngine");
    qmlRegisterType<AlbumImage>("com.alientek.qmlcomponents", 1, 0, "AlbumImage");

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject *object, const QUrl &createdUrl)
                     {
        if (!object && createdUrl == url)
            QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(url);
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
