#ifndef MUSICENGINE_H
#define MUSICENGINE_H

#include "data/SongData.h"
#include "player/PlayerState.h"

#include <QObject>
#include <QString>
#include <QUrl>

class AlsaPlayer;
class Downloader;
class MusicApi;

// QML 可调用的选歌、联网准备和本地播放总控制器
class MusicEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool seekable READ seekable NOTIFY seekableChanged)
    Q_PROPERTY(bool hasAudio READ hasAudio NOTIFY hasAudioChanged)
    Q_PROPERTY(bool foregroundAllowed READ foregroundAllowed
                   WRITE setForegroundAllowed NOTIFY foregroundAllowedChanged)
    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(int error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QString songName READ songName NOTIFY songChanged)
    Q_PROPERTY(QString singer READ singer NOTIFY songChanged)
    Q_PROPERTY(QString albumName READ albumName NOTIFY songChanged)
    Q_PROPERTY(QString coverPath READ coverPath NOTIFY coverPathChanged)
public:
    // 对 QML 暴露的播放状态
    enum ReplaceState
    {
        StoppedState,
        PlayingState,
        PausedState
    };
    Q_ENUM(ReplaceState)
    // 对 QML 暴露的文件准备状态
    enum ReplaceStatus
    {
        NoMedia,
        LoadingMedia,
        LoadedMedia,
        EndOfMedia,
        InvalidMedia
    };
    Q_ENUM(ReplaceStatus)
    // 对 QML 暴露的错误分类
    enum ReplaceError
    {
        NoError,
        ResourceError,
        FormatError,
        NetworkError
    };
    Q_ENUM(ReplaceError)

    // 创建网络、下载和本地播放子对象
    explicit MusicEngine(QObject *parent = nullptr);
    // 最终取消请求并回收播放线程
    ~MusicEngine() override;

    int playbackState() const;                           // 返回供播放按钮使用的状态
    qint64 position() const;                             // 返回当前播放位置，单位毫秒
    qint64 duration() const;                             // 返回当前时长，单位毫秒
    qreal volume() const;                                // 返回 0～1 范围的界面音量
    void setVolume(qreal value);                         // 设置 0～1 范围的界面音量
    bool seekable() const;                               // 判断当前本地文件是否可 seek
    bool hasAudio() const;                               // 判断当前会话是否已准备好本地音频
    bool foregroundAllowed() const;                      // 判断窗口是否允许开始播放
    Q_INVOKABLE void setForegroundAllowed(bool allowed); // 隐藏或最小化时停止，重新显示不自动播放
    int status() const;                                  // 返回文件准备状态
    int error() const;                                   // 返回最近错误分类
    QString source() const;                              // 返回当前歌曲 ID 或本地路径
    void setSource(const QString &source);               // 选择并准备歌曲，不自动播放
    QString songName() const;
    QString singer() const;
    QString albumName() const;
    QString coverPath() const;

    // 记录播放意图，在文件就绪且窗口可见时启动
    Q_INVOKABLE void play();
    // 暂停当前播放
    Q_INVOKABLE void pause();
    // 手动停止并把位置归零
    Q_INVOKABLE void stop();
    // 在本地文件就绪后跳转到毫秒位置
    Q_INVOKABLE void seek(qint64 positionMs);
    // 选择预置列表中的歌曲并播放
    Q_INVOKABLE void playIndex(int index);
    // 应用退出时取消请求并回收音频资源
    Q_INVOKABLE void shutdown();

signals:
    // 播放状态变化
    void playbackStateChanged(int state);
    // 毫秒位置变化
    void positionChanged(qint64 positionMs);
    // 毫秒时长变化
    void durationChanged(qint64 durationMs);
    // 界面音量变化
    void volumeChanged(qreal volume);
    // seek 权限变化
    void seekableChanged(bool seekable);
    // 本地音频就绪状态变化
    void hasAudioChanged(bool hasAudio);
    // 窗口前台播放许可变化
    void foregroundAllowedChanged(bool allowed);
    // 文件准备状态变化
    void statusChanged(int status);
    // 最近错误分类变化
    void errorChanged(int error);
    // 所选歌曲身份变化
    void sourceChanged(const QString &source);
    // 歌曲标题、歌手或专辑发生变化
    void songChanged();
    // 封面 URL 变化
    void coverPathChanged();
    // 当前歌曲歌词已就绪，也用于清空旧歌词
    void lyricReady(const QString &text);
    // 当前会话的错误说明
    void errorMessage(const QString &message);
    // 只有当前会话自然结束时触发下一首
    void naturalPlaybackEnded();

private:
    // 查找预置歌曲 ID，未找到返回 -1
    int songIndexForId(const QString &id) const;
    // 检查当前网络或播放器结果的会话身份
    bool isCurrent(quint64 sessionId, const QString &songId) const;
    // 取消旧会话尚未完成的网络请求
    void cancelNetwork();
    // 从缓存加载或请求当前歌曲 MP3
    void loadCurrentSong();
    // 将已准备好的本地路径交给播放器
    void prepareLocalFile(const QString &path, quint64 sessionId);
    // 并行请求歌词和封面
    void requestMetadata();
    // 更新当前本地音频就绪标志
    void setHasAudio(bool ready);
    // 更新当前可 seek 标志
    void setSeekable(bool ready);
    // 更新播放状态并通知 QML
    void setPlaybackState(int state);
    // 更新文件准备状态并通知 QML
    void setStatus(int status);
    // 更新错误分类并通知 QML
    void setError(int error, const QString &message);

    AlsaPlayer *m_player = nullptr;
    MusicApi *m_api = nullptr;
    Downloader *m_downloader = nullptr;
    QList<Song> m_songs;
    Song m_current;
    QString m_source;
    QString m_localFile;
    QString m_coverPath;
    quint64 m_sessionId = 0;
    quint64 m_boundPlayerSession = 0;
    qint64 m_position = 0;
    qint64 m_duration = 0;
    int m_playbackState = StoppedState;
    int m_status = NoMedia;
    int m_error = NoError;
    int m_volumePercent = 80;
    bool m_hasAudio = false;
    bool m_seekable = false;
    bool m_wantsPlay = false;
    bool m_foregroundAllowed = true;
    bool m_shutdown = false;
};

#endif // MUSICENGINE_H
