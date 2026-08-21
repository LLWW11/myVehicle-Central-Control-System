/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is based on the Qt Location Map Viewer example.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>

#include "seriallocation.h"
#include "systemuicommonapiclient.h"

/**
 * @brief 创建地图应用、串口定位对象并加载综合桌面客户端界面。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return 应用退出码；QML 根对象创建失败时返回 -1。
 */
int main(int argc, char *argv[])
{
    // 在创建应用对象前启用高 DPI 缩放
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication application(argc, argv);

    SerialLocation serialLocation;

    QQmlApplicationEngine engine;

    // 注册综合桌面通信客户端，供 QML 实例化。
    qmlRegisterType<SystemUICommonApiClient>(
        "com.alientek.qmlcomponents", 1, 0, "SystemUICommonApiClient");

    engine.rootContext()->setContextProperty(
        QStringLiteral("serialLocation"),
        &serialLocation);

    serialLocation.openPort(
        QStringLiteral("/dev/ttymxc2"),
        115200);

    engine.load(QUrl(QStringLiteral("qrc:/mapviewer.qml")));

    // QML 加载失败时不进入一个没有界面的事件循环。
    if (engine.rootObjects().isEmpty())
        return -1;

    return application.exec();
}
