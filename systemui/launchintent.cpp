/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @brief         launchintent.cpp
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @date          2023-11-08
* @link          http://www.openedv.com/forum.php
*******************************************************************/

#include <QGuiApplication>
#include "launchintent.h"
#include <QDebug>
#include <QFile>

LaunchIntent::LaunchIntent(QObject *parent)
    : QObject{parent}
{

}

void LaunchIntent::lauchApp(const QString &appName)
{
    QString cmd = QGuiApplication::applicationDirPath() + "/src/apps/" + appName;
    QFile file(cmd);
    if (file.exists()) {
        QProcess tmpPro;
        tmpPro.start("pidof " + appName);
        tmpPro.waitForFinished(100); // default 100
        if (!tmpPro.exitCode()) {
            // If it is already running, then we are no longer running, perhaps the App is stuck and unresponsive
            qDebug() << "似乎已经运行" + appName << cmd;
            emit noAppFile();
            return;
        }
        QProcess *pro =  new QProcess();
        pro->start(cmd);
        pro->setObjectName(appName);
        connect(pro, SIGNAL(finished(int)), this, SLOT(onAppExitHandler(int)));
        connect(pro, SIGNAL(finished(int)), pro, SLOT(deleteLater()));
        //connect(pro, SIGNAL(finished(int)), this, SIGNAL(noAppFile()));
        qDebug() << cmd;
    } else {
        emit noAppFile();
        qDebug() << "无法找到" << cmd;
    }
}

void LaunchIntent::onAppExitHandler(int exitValue)
{
    emit appExitHandler((QProcess*)sender(), exitValue);
}
