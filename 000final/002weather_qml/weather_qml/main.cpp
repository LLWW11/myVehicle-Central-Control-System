/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @brief         weather_qml main.cpp
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @date          2024-09-05
* @link          http://www.openedv.com/forum.php
*******************************************************************/

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QCoreApplication>

#include "systemuicommonapiclient.h"
#include "weather.h"

/**
 * @brief 创建天气应用、注册 QML 类型并加载综合桌面客户端界面。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return 应用事件循环退出码；QML 根对象创建失败时返回非零值。
 */
int main(int argc, char *argv[])
{
    // Qt 5 高 DPI 适配(正点原子官方风格)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("weather_qml"));
    app.setOrganizationName(QStringLiteral("ALIENTEK"));

    // 注册业务类型和综合桌面通信客户端，供 QML 实例化。
    qmlRegisterType<Weather>("com.alientek.qmlcomponents", 1, 0, "Weather");
    qmlRegisterType<SystemUICommonApiClient>(
        "com.alientek.qmlcomponents", 1, 0, "SystemUICommonApiClient");

    QQmlApplicationEngine engine;

    // 暴露可执行文件目录,QML 端可用 appCurrtentDir 访问本地文件
    engine.rootContext()->setContextProperty(
        QStringLiteral("appCurrtentDir"),
        QCoreApplication::applicationDirPath());

    const QUrl mainUrl(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [mainUrl](QObject *object, const QUrl &objectUrl) {
        if (object == nullptr && objectUrl == mainUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(mainUrl);
    return app.exec();
}
