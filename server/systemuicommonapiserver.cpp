/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @projectName   server
* @brief         systemuicommonApi.cpp
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @date          2023-10-31
* @link          http://www.openedv.com/forum.php
*******************************************************************/
#include "systemuicommonapiserver.h"
#include <QDebug>
#include <QEventLoop>
#include <QTimer>

const QString Properties::appName("appName");
const QString Properties::appState("appState");
const QString Properties::command("command");


SystemUICommonApiServer::SystemUICommonApiServer(QObject *parent):
    SystemUICommonApiSource(parent),
    m_appIsRunning(false),
    m_currtentLauchAppName(""),
    m_currtentAppIsActive(false)
{
    m_remoteObjectHost = new QRemoteObjectHost(this);
    m_remoteObjectHost->setHostUrl(QUrl(QStringLiteral("local:interfaces")));
    m_remoteObjectHost->enableRemoting(this);
    detectAppIsAlreadyRunningTimer =  new QTimer(this);
    // default 20，If the running App does not respond within Xms,
    // we will start a new process, and before starting a new process,
    // we will check whether any app has been started
    detectAppIsAlreadyRunningTimer->setInterval(50);
    connect(detectAppIsAlreadyRunningTimer, SIGNAL(timeout()), this, SLOT(timeOutDetectIsAlreadyRunning()));
    launchIntent = new LaunchIntent(this);
    connect(launchIntent, SIGNAL(noAppFile()), this, SLOT(noAppFile()));
    connect(launchIntent, SIGNAL(appExitHandler(QProcess*, int)), this, SLOT(onAppExitHandler(QProcess*, int)));
}

QString SystemUICommonApiServer::currtentLauchAppName()
{
    return m_currtentLauchAppName;
}

void SystemUICommonApiServer:: onServerVariant(const QByteArray &data)
{
    updateReceiveAppMessages(deserializeSystemUIMessages(data));
}

void SystemUICommonApiServer::quitNotification(QString appName)
{
    SystemUIMessages msg;
    msg[pros.appName] = appName;
    // Ask what state the App is in when it first starts
    msg[pros.command] = Command::Quit;
    QByteArray ba = serializeSystemUIMessages(msg);
    serverSendVariant(ba);
}

void SystemUICommonApiServer::returnToDesktop()
{
    // 桌面已经显示时无需结束可能仍在后台运行的应用。
    if (m_currtentLauchAppName.isEmpty() || m_currtentLauchAppName == "null") {
        Q_EMIT requestVisibilityChange(Command::Show);
        return;
    }

    // 应用退出回调会恢复并显示桌面，避免录像等任务被强制中断。
    quitNotification(m_currtentLauchAppName);
}

void SystemUICommonApiServer::launchApp(const QString &appName)
{
    setCurrtentLauchAppName(appName);
    SystemUIMessages msg;
    msg[pros.appName] = appName;
    // Ask what state the App is in when it first starts
    msg[pros.appState] = AppState::WhichState;
    QByteArray ba = serializeSystemUIMessages(msg);
    serverSendVariant(ba);
    detectAppIsAlreadyRunningTimer->start();
}

void SystemUICommonApiServer::setCurrtentLauchAppName(const QString &appName)
{
    if (appName != m_currtentLauchAppName) {
        m_currtentLauchAppName = appName;
        Q_EMIT currtentLauchAppNameChanged();
        detectAppIsAlreadyRunningTimer->stop();
        if (m_currtentLauchAppName != "") {
            detectAppIsAlreadyRunningTimer->start();
            setColdLaunch(true);
        }

        setAppIsRunning(false);
        setCurrtentAppIsActive(false);
    }
}

void SystemUICommonApiServer::timeOutDetectIsAlreadyRunning()
{
    detectAppIsAlreadyRunningTimer->stop();
    setColdLaunch(true);
    launchIntent->lauchApp(m_currtentLauchAppName);
}

void SystemUICommonApiServer::noAppFile()
{
    setColdLaunch(true);
    setCurrtentLauchAppName("null");
    Q_EMIT appIsUnistalled();
}

void SystemUICommonApiServer::onAppExitHandler(QProcess *process, int exitValue)
{
    if (m_currtentLauchAppName == process->objectName()) {
        setCurrtentLauchAppName("null");
        setColdLaunch(true);
        Q_EMIT requestVisibilityChange(Command::Show);
    }
    if (exitValue != 0) {
        Q_EMIT appCrashHandler(process->objectName());
    }
}

QByteArray SystemUICommonApiServer::serializeSystemUIMessages(const SystemUIMessages &messages)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);

    quint32 size = messages.size();
    stream << size;

    QMap<QString, QVariant>::const_iterator it = messages.constBegin();

    while (it != messages.constEnd()) {
        QString key = it.key();
        QVariant value = it.value();
        stream << key;
        stream << value;
        ++it;
    }

    return data;
}

SystemUIMessages SystemUICommonApiServer::deserializeSystemUIMessages(const QByteArray &data)
{
    SystemUIMessages messages;
    QByteArray ba = data;
    QDataStream stream(&ba, QIODevice::ReadOnly);

    quint32 size;
    stream >> size;

    for (quint32 i = 0; i < size; ++i) {
        QString key;
        QVariant value;
        stream >> key >> value;
        messages[key] = value;
    }

    return messages;
}

bool SystemUICommonApiServer::coldLaunch() const
{
    return m_coldLaunch;
}

void SystemUICommonApiServer::setColdLaunch(bool newColdLaunch)
{
    if (m_coldLaunch == newColdLaunch)
        return;
    m_coldLaunch = newColdLaunch;
    Q_EMIT coldLaunchChanged();
}

void SystemUICommonApiServer::updateReceiveAppMessages(SystemUIMessages messages)
{
    SystemUIMessages::const_iterator it = messages.constBegin();
    while (it != messages.constEnd()) {
        updateReceiveAppMessages(it.key(), it.value());
        ++it;
    }

    m_propertiesCache[pros.appState] = m_receiveAppMessagesCache.value(pros.appState);


    if (m_currtentLauchAppName == m_receiveAppMessagesCache.value(pros.appName)) {
        if (m_receiveAppMessagesCache.value(pros.appState) == AppState::Background) {
            qDebug() <<  m_currtentLauchAppName << "is running in background";
        }
        if (m_receiveAppMessagesCache.value(pros.appState) == AppState::Active) {
            qDebug() <<  m_currtentLauchAppName << "app is starting";
        }
        if (m_receiveAppMessagesCache.value(pros.appState) == AppState::Background ||
                m_receiveAppMessagesCache.value(pros.appState) == AppState::Active) {
            setColdLaunch(false);
            detectAppIsAlreadyRunningTimer->stop();
            setAppIsRunning(true);
            setCurrtentAppIsActive(true);
            Q_EMIT requestVisibilityChange(Command::Hide);
            m_propertiesCache[pros.appName] = m_currtentLauchAppName;
            serverSendVariant(serializeSystemUIMessages(m_propertiesCache));
            //m_currtentLauchAppName = "";
        }
    }/* else if (m_receiveAppMessagesCache.value(pros.appState) == AppState::Active) {
        Q_EMIT requestVisibilityChange(Command::Hide);
    }*/

    if (m_receiveAppMessagesCache.value(pros.command) == Command::Show) {
        Q_EMIT requestVisibilityChange(Command::Show);
        detectAppIsAlreadyRunningTimer->stop();
        setColdLaunch(true);
    }

    m_receiveAppMessagesCache.clear();

}

void SystemUICommonApiServer::updateReceiveAppMessages(const QString &name, const QVariant &value)
{
    m_receiveAppMessagesCache.insert(name, value);
}

bool SystemUICommonApiServer::currtentAppIsActive() const
{
    return m_currtentAppIsActive;
}

void SystemUICommonApiServer::setCurrtentAppIsActive(bool newCurrtentAppIsActive)
{
    if (m_currtentAppIsActive == newCurrtentAppIsActive)
        return;
    m_currtentAppIsActive = newCurrtentAppIsActive;
    Q_EMIT currtentAppIsActiveChanged();
}

bool SystemUICommonApiServer::appIsRunning() const
{
    return m_appIsRunning;
}

void SystemUICommonApiServer::setAppIsRunning(bool newAppIsRunning)
{
    if (m_appIsRunning == newAppIsRunning)
        return;
    m_appIsRunning = newAppIsRunning;
    Q_EMIT appIsRunningChanged();
}
