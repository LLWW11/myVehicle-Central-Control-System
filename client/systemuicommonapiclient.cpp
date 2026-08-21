/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @projectName   client
* @brief         systemuicommonapiclient.cpp
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @date          2023-10-31
* @link          http://www.openedv.com/forum.php
*******************************************************************/
#include "systemuicommonapiclient.h"
#include <QDebug>
#include <QRemoteObjectSourceLocation>
#include <QRandomGenerator>
#include <QtCore>

#include <QRegularExpression>
#include <QRegularExpressionMatch>

const QString Properties::appName("appName");
const QString Properties::appX("appX");
const QString Properties::appY("appY");
const QString Properties::appIconWidth("appIconWidth");
const QString Properties::appIconHeight("appIconHeight");
const QString Properties::appIconPath("appIconPath");
const QString Properties::pageIndex("pageIndex");
const QString Properties::appState("appState");
const QString Properties::command("command");
const QString Properties::appStateImage("appStateImage");
const QString Properties::launchMode("launchMode");

QString SystemUICommonApiClient::appName() const
{
    return m_appName;
}

void SystemUICommonApiClient::setAppName(const QString &appName)
{
    if (appName == m_appName)
        return;
    m_appName = appName;
    emit appNameChanged();
    onSystemuiconfChanged();
}

SystemUICommonApiClient::SystemUICommonApiClient(QObject *parent) : QObject(parent), m_appName("")
{
    timer = new QTimer();
    timer->start(100);
    connect(timer, SIGNAL(timeout()), this, SLOT(timerTimeOut()));
    m_remoteObjectNode = new QRemoteObjectNode(this);
    // QStringLiteral
    m_remoteObjectNode->connectToNode(QUrl(QStringLiteral("local:interfaces")));

    m_systemUICommonApiReplica = m_remoteObjectNode->acquire<SystemUICommonApiReplica>();

    connect(m_systemUICommonApiReplica, SIGNAL(serverSendVariant(QByteArray)), this, SLOT(onServerVariant(QByteArray)));

    m_systemuiConfSystemWatcher = new QFileSystemWatcher(this);
    QFile file(QCoreApplication::applicationDirPath() + "/resource/systemui.conf");
    if (file.exists()) {
        m_systemuiConfSystemWatcher->addPath(file.fileName());
        connect(m_systemuiConfSystemWatcher, SIGNAL(fileChanged(QString)), this, SLOT(onSystemuiconfChanged()));
    }
}

void SystemUICommonApiClient::sendAppStateImage(QVariant variant)
{
    QByteArray byteArray;
    if (variant.canConvert<QImage>()) {
        QBuffer buffer;
        buffer.open(QIODevice::WriteOnly);
        variant.value<QImage>().save(&buffer, "JPG");
        byteArray = buffer.data();
        SystemUIMessages msg;
        msg[pros.appName] = m_appName;
        msg[pros.appStateImage] = byteArray;
        QByteArray byte = serializeSystemUIMessages(msg);
        m_systemUICommonApiReplica->onServerVariant(byte);
        buffer.close();
    }
}

void SystemUICommonApiClient::updateProperties(SystemUIMessages properties)
{
    if (properties.value(pros.appName) != m_appName)
        return;

    SystemUIMessages::const_iterator it = properties.constBegin();
    while (it != properties.constEnd()) {
        updatePropertyCache(it.key(), it.value());
        ++it;
    }
    if (!m_propertiesCache.value(pros.pageIndex).isNull()) {
        static bool index = m_propertiesCache.value(pros.pageIndex).toInt();
        if (index != m_propertiesCache.value(pros.pageIndex).toInt())
            setColdPageIndex(false);
        else
            setColdPageIndex(true);
    }

    if (m_propertiesCache.value(pros.appState) == AppState::WhichState) {
        SystemUIMessages msg;
        msg[pros.appName] = m_appName;
        msg[pros.appState] = AppState::Background;
        QByteArray byte = serializeSystemUIMessages(msg);
        m_systemUICommonApiReplica->onServerVariant(byte);
    }

    if (m_propertiesCache.value(pros.appState) == AppState::Background ||
            m_propertiesCache.value(pros.appState) == AppState::Active) {
        emit appAppPropertyChanged();
        emit actionCommand(Command::Show);
    }

    if (m_propertiesCache.value(pros.command) == Command::Quit ) {
        emit actionCommand(Command::Quit);
    }

    m_propertiesCache.remove(pros.appState);
}

QByteArray SystemUICommonApiClient::serializeSystemUIMessages(const SystemUIMessages &messages)
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

SystemUIMessages SystemUICommonApiClient::deserializeSystemUIMessages(const QByteArray &data)
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

void SystemUICommonApiClient::updatePropertyCache(const QString &name, const QVariant &value)
{
    if (m_propertiesCache.value(name) == value)
        return;
    if (value.isNull())
        return;
    m_propertiesCache.insert(name, value);

    if (name == pros.appX) {
        emit appXChanged();
    } else if (name == pros.appY) {
        emit appXChanged();
    } else if (name == pros.appIconWidth) {
        emit appIconWidthChanged();
    } else if (name == pros.appIconHeight) {
        emit appIconHeightChanged();
    } else if (name == pros.appIconPath) {
        emit appIconPathChanged();
    } else if (name == pros.pageIndex) {
        emit pageIndexChanged();
    }  else if (name == pros.launchMode) {
        emit lanuchModeChanged();
    }
}

bool SystemUICommonApiClient::backgroundTask() const
{
    return m_backgroundTask;
}

void SystemUICommonApiClient::setBackgroundTask(bool newBackgroundTask)
{
    if (m_backgroundTask == newBackgroundTask)
        return;
    m_backgroundTask = newBackgroundTask;
    emit backgroundTaskChanged();
}

bool SystemUICommonApiClient::applicationAnimation() const
{
    return m_applicationAnimation;
}

void SystemUICommonApiClient::setApplicationAnimation(bool newApplicationAnimation)
{
    if (m_applicationAnimation == newApplicationAnimation)
        return;
    m_applicationAnimation = newApplicationAnimation;
    emit applicationAnimationChanged();
}

bool SystemUICommonApiClient::coldPageIndex() const
{
    return m_coldPageIndex;
}

void SystemUICommonApiClient::setColdPageIndex(bool newColdPageIndex)
{
    if (m_coldPageIndex == newColdPageIndex)
        return;
    m_coldPageIndex = newColdPageIndex;
    emit coldPageIndexChanged();
}

int SystemUICommonApiClient::lanuchMode() const
{
    return m_propertiesCache.value(pros.launchMode).toInt();
}

int SystemUICommonApiClient::pageIndex() const
{
    return m_propertiesCache.value(pros.pageIndex).toInt();
}

QString SystemUICommonApiClient::appIconPath() const
{
    return m_propertiesCache.value(pros.appIconPath).toString();
}

qreal SystemUICommonApiClient::appIconHeight() const
{
    return m_propertiesCache.value(pros.appIconHeight).toReal();
}

qreal SystemUICommonApiClient::appIconWidth() const
{
    return m_propertiesCache.value(pros.appIconWidth).toReal();
}

qreal SystemUICommonApiClient::appY() const
{
    return m_propertiesCache.value(pros.appY).toReal();
}

qreal SystemUICommonApiClient::appX() const
{
    return m_propertiesCache.value(pros.appX).toReal();
}

void SystemUICommonApiClient::onServerVariant(const QByteArray &ba)
{
    updateProperties(deserializeSystemUIMessages(ba));
}

void SystemUICommonApiClient::timerTimeOut()
{
    timer->stop();
    delete timer;
    SystemUIMessages msg;
    msg[pros.appName] = m_appName;
    msg[pros.appState] = AppState::Active;
    QByteArray byte = serializeSystemUIMessages(msg);
    m_systemUICommonApiReplica->onServerVariant(byte);
}

void SystemUICommonApiClient::onSystemuiconfChanged()
{
    if (m_systemuiConfSystemWatcher->files().length() == 0)
        return;
    // 判断是否为Nandflash核心板
    QFile file("/dev/mtd0");
    if (file.exists()) {
        return;
    }
    file.setFileName((m_systemuiConfSystemWatcher->files()[0]));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString str = file.readAll();
        if (str.contains("application-animation=false"))
            setApplicationAnimation(false);
        else
            setApplicationAnimation(true);

        if (str.contains("background-task=all")) {
            setBackgroundTask(true);
            return;
        }
        QRegularExpression reg;
        QRegularExpressionMatch match;

        foreach (QString tmpStr, str.split("\n")) {
            reg.setPattern("background-task=([^\\s]+)");
            match = reg.match(tmpStr, 0);
            if (match.hasMatch()) {
                QString tmpStr2 = match.captured(1).simplified();
                if (tmpStr2.contains(",")) {
                    foreach (QString tmpStr3, tmpStr2.split(",")) {
                        if (m_appName == tmpStr3) {
                            setBackgroundTask(true);
                            return;
                        }
                    }
                } else {
                    if (m_appName == tmpStr2) {
                        setBackgroundTask(true);
                        return;
                    }
                }
                setBackgroundTask(false);
            }
        }

        file.close();
    }
}

void SystemUICommonApiClient::askSystemUItohideOrShow(Command cmd)
{
    SystemUIMessages msg;
    msg[pros.command] = cmd;
    QByteArray byte = serializeSystemUIMessages(msg);
    m_systemUICommonApiReplica->onServerVariant(byte);
}


