#ifndef ALSAPLAYER_H
#define ALSAPLAYER_H

/*
 * AlsaPlayer - Qt 与纯 C ALSA 引擎之间的桥接层(仅 Linux 编译)
 *
 * 线程分布:
 *   - GUI 线程: 本对象, 命令转发 + QTimer 轮询进度
 *   - 引擎播放线程(C pthread): 解码/写出, 通过 C 回调 Q_EMIT Qt 信号
 *     (信号自动排队到 GUI 线程, 无需手动加锁)
 *   - 下载线程: Downloader 内部 QNetworkAccessManager 异步下载
 *
 * URL 处理:
 *   - 本地文件直接交给 C 引擎
 *   - http(s) URL 先下载到临时目录(缓存到 md5 文件名), 再交给 C 引擎
 */
#include <QObject>
#include <QTimer>

#include "player/PlayerState.h"

class Downloader;

class AlsaPlayer : public QObject {
    Q_OBJECT
public:
    explicit AlsaPlayer(QObject* parent = nullptr);
    ~AlsaPlayer() override;

    void setUrl(const QString& url);
    void playUrl(const QString& url, const QString& destFile);
    void setDownloadDir(const QString& dir);
    void play();
    void pause();
    void stop();
    void setVolume(int v);
    void seek(qint64 ms);

    PlayerState state() const;
    qint64 position() const;
    qint64 duration() const;

Q_SIGNALS:
    void stateChanged(PlayerState state);
    void positionChanged(qint64 ms);
    void durationChanged(qint64 ms);
    void errorOccurred(const QString& msg);

private Q_SLOTS:
    void onPollTimer();
    void onDownloadFinished(const QString& path);
    void onDownloadFailed(const QString& msg);

private:
    void startPlayback(const QString& path);

    Downloader* m_downloader;
    QTimer* m_timer;
    QString m_downloadDir;
    QString m_currentFile;
    bool m_hasFile = false;
    PlayerState m_lastState = PlayerState::Stopped;
    qint64 m_lastPos = -1;
    qint64 m_lastDur = -1;
};

#endif // ALSAPLAYER_H
