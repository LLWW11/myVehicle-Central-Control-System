#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTextCodec>
#include <QQmlContext>
#include <QFile>
#include "apklistmodel.h"
#include "systemuicommonapiserver.h"
#include "launchintent.h"
#include "keyinputeventthread.h"

/**
 * @brief 初始化 SystemUI、注册 QML 类型并进入主事件循环。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return 应用程序退出码。
 */
int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QGuiApplication app(argc, argv);
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    QLocale locale(QLocale::Chinese);
    QLocale::setDefault(locale);

    QString hostName;
    QFile file("/etc/hostname");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        hostName =  file.readLine().simplified();
        file.close();
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appCurrtentDir", QCoreApplication::applicationDirPath());
    engine.rootContext()->setContextProperty("hostName", hostName);
    qmlRegisterType<ApkListModel>("com.alientek.qmlcomponents", 1, 0, "ApkListModel");
    qmlRegisterType<SystemUICommonApiServer>("com.alientek.qmlcomponents", 1, 0, "SystemUICommonApiServer");
    qmlRegisterType<KeyInputEventThread>("com.alientek.qmlcomponents", 1, 0, "KeyInputEventThread");
    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.load(url);
    return app.exec();
}
