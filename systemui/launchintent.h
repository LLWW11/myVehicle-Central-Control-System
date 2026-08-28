/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @brief         launchintent.h
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @date          2023-11-08
* @link          http://www.openedv.com/forum.php
*******************************************************************/
#ifndef LAUNCHINTENT_H
#define LAUNCHINTENT_H

#include <QObject>
#include <QProcess>

class LaunchIntent : public QObject
{
    Q_OBJECT
public:
    explicit LaunchIntent(QObject *parent = nullptr);

Q_SIGNALS:
    void noAppFile();
    void appExitHandler(QProcess *pro, int exitValue);

public Q_SLOTS:
    void lauchApp(const QString &appName);
private Q_SLOTS:
    void onAppExitHandler(int exitValue);

};

#endif // LAUNCHINTENT_H
