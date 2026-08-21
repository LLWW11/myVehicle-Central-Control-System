/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @projectName   client
* @brief         systemuicommonapiclient.h
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @date          2023-10-31
* @link          http://www.openedv.com/forum.php
*******************************************************************/
#ifndef SYSTEMUICOMMONAPICLIENT_H
#define SYSTEMUICOMMONAPICLIENT_H

#include <QObject>
#include <QTimer>
#include <QImage>
#include "rep_systemuicommonapi_replica.h"
#include <QRemoteObjectNode>
#include <QByteArray>
#include <QVariant>
#include <QMap>
#include <QFileSystemWatcher>
#include <QVector>

typedef QMap<QString, QVariant> SystemUIMessages;

class Properties {
public:
    static const QString appName;
    static const QString appX;
    static const QString appY;
    static const QString appIconWidth;
    static const QString appIconHeight;
    static const QString appIconPath;
    static const QString pageIndex;
    static const QString appState;
    static const QString command;
    static const QString appStateImage;
    static const QString launchMode;
};

class SystemUICommonApiClient : public QObject
{
    Q_OBJECT
    Q_ENUMS(Command)
    Q_ENUMS(AppState)
    Q_ENUMS(LaunchMode)
public:
    explicit SystemUICommonApiClient(QObject *parent = nullptr);

    Q_PROPERTY(QString appName READ appName WRITE setAppName NOTIFY appNameChanged)
    Q_PROPERTY(qreal appX READ appX NOTIFY appXChanged)
    Q_PROPERTY(qreal appY READ appY NOTIFY appYChanged)
    Q_PROPERTY(qreal appIconWidth READ appIconWidth NOTIFY appIconWidthChanged)
    Q_PROPERTY(qreal appIconHeight READ appIconHeight NOTIFY appIconHeightChanged)
    Q_PROPERTY(QString appIconPath READ appIconPath NOTIFY appIconPathChanged)
    Q_PROPERTY(int pageIndex READ pageIndex NOTIFY pageIndexChanged)
    Q_PROPERTY(int lanuchMode READ lanuchMode NOTIFY lanuchModeChanged)
    Q_PROPERTY(bool coldPageIndex READ coldPageIndex WRITE setColdPageIndex NOTIFY coldPageIndexChanged FINAL)
    Q_PROPERTY(bool applicationAnimation READ applicationAnimation NOTIFY applicationAnimationChanged FINAL)
    Q_PROPERTY(bool backgroundTask READ backgroundTask NOTIFY backgroundTaskChanged FINAL)

    enum Command {
        UnknowCommand,
        Show,
        Quit,
        Hide
    };

    enum AppState {
        UnknowState = 0,
        Background,
        Active,
        WhichState
    };

    enum LaunchMode {
        UnknowMode = 0,
        ClickIcon,
        AppSwitcher
    };

    Properties pros;
    // The properties of the app when it is clicked
    SystemUIMessages m_propertiesCache;

    void updateProperties(SystemUIMessages properties);

    QString appName() const;
    void setAppName(const QString &appName);

    qreal appX() const;
    qreal appY() const;
    qreal appIconWidth() const;
    qreal appIconHeight() const;
    QString appIconPath() const;
    int pageIndex() const;
    int lanuchMode() const;

    bool coldPageIndex() const;
    void setColdPageIndex(bool newColdPageIndex);

    bool applicationAnimation() const;
    void setApplicationAnimation(bool newApplicationAnimation);

    bool backgroundTask() const;
    void setBackgroundTask(bool newBackgroundTask);
signals:
    void appNameChanged();
    void actionCommand(Command cmd);
    void appAppPropertyChanged();

    void appXChanged();
    void appYChanged();
    void appIconWidthChanged();
    void appIconHeightChanged();
    void appIconPathChanged();
    void pageIndexChanged();
    void lanuchModeChanged();

    void coldPageIndexChanged();

    void applicationAnimationChanged();

    void backgroundTaskChanged();

private:
    QByteArray serializeSystemUIMessages(const SystemUIMessages &messages);
    SystemUIMessages deserializeSystemUIMessages(const QByteArray& data);

    void updatePropertyCache(const QString &name, const QVariant& value);

    QRemoteObjectNode * m_remoteObjectNode = nullptr;
    SystemUICommonApiReplica * m_systemUICommonApiReplica = nullptr;
    QString m_appName;
    QTimer *timer;
    bool m_coldPageIndex;
    bool m_applicationAnimation;

    QFileSystemWatcher *m_systemuiConfSystemWatcher;

    bool m_backgroundTask;

public slots:
    void askSystemUItohideOrShow(Command cmd);
    void sendAppStateImage(QVariant Variant);
    void onSystemuiconfChanged();
private slots:
    void onServerVariant(const QByteArray &ba);
    void timerTimeOut();
};

#endif // SYSTEMUICOMMONAPICLIENT_H
