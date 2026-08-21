#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>

#include "cameracontroller.h"
#include "camera/framestore.h"
#include "camera/previewimageprovider.h"
#include "systemuicommonapiclient.h"

/**
 * @brief 创建应用、注册摄像头控制器与图像提供器，并加载 QML 主界面。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return Qt 应用退出码。
 */
int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("camrecorder"));
    app.setOrganizationName(QStringLiteral("ALIENTEK"));

    FrameStore frameStore;
    CameraController controller(&frameStore);
    QQmlApplicationEngine engine;

    // 注册综合桌面通信客户端，供 QML 实例化。
    qmlRegisterType<SystemUICommonApiClient>(
        "com.alientek.qmlcomponents", 1, 0, "SystemUICommonApiClient");

    engine.addImageProvider(QStringLiteral("camera"),
                            new PreviewImageProvider(&frameStore));
    engine.rootContext()->setContextProperty(QStringLiteral("cameraController"),
                                             &controller);

    const QUrl mainUrl(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [mainUrl](QObject *object, const QUrl &objectUrl) {
        if (object == nullptr && objectUrl == mainUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(mainUrl);
    return app.exec();
}
