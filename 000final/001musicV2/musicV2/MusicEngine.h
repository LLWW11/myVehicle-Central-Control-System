#ifndef MUSICENGINE_H
#define MUSICENGINE_H

#include <QObject>
#include <QString>

#include "data/SongData.h"
#include "player/PlayerState.h"

class AlsaPlayer;
class MusicApi;

/*
 * MusicEngine - 供 QML 调用的播放引擎门面。
 * 对外模拟 QMediaPlayer 的常用属性/方法(playbackState/position/duration/
 * volume/seekable/hasAudio/autoPlay/status/seek/play/pause...),
 * 使原 UI 的 musicPlayer 控件可以无痛替换。
 *
 * 内部串联:
 *   MusicApi    - 动态解析播放 URL / 拉歌词 / 下载封面
 *   MusicCache  - 所有下载内容落盘 <appDir>/cache/
 *   AlsaPlayer  - 纯 C ALSA + mpg123 播放引擎
 */
class MusicEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(int playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool seekable READ seekable CONSTANT)
    Q_PROPERTY(bool hasAudio READ hasAudio NOTIFY hasAudioChanged)
    Q_PROPERTY(bool autoPlay READ autoPlay WRITE setAutoPlay NOTIFY autoPlayChanged)
    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(int error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QString songName READ songName NOTIFY songChanged)
    Q_PROPERTY(QString singer READ singer NOTIFY songChanged)
    Q_PROPERTY(QString albumName READ albumName NOTIFY songChanged)
    Q_PROPERTY(QString coverPath READ coverPath NOTIFY coverPathChanged)
public:
    // 与 QMediaPlayer::Status 取值一致, 供 QML MediaPlayer.<Status> 判断
    enum ReplaceStatus {
        UnknownMediaStatus = 0,
        NoMedia = 1,
        LoadingMedia = 2,
        LoadedMedia = 3,
        BufferingMedia = 4,
        StalledMedia = 5,
        BufferedMedia = 6,
        EndOfMedia = 7,
        InvalidMedia = 8
    };
    // 与 QMediaPlayer::State 取值一致
    enum ReplaceState {
        StoppedState = 0,
        PlayingState = 1,
        PausedState = 2
    };
    // 与 QMediaPlayer::Error 取值一致
    enum ReplaceError {
        NoError = 0,
        ResourceError = 1,
        FormatError = 2,
        NetworkError = 3,
        AccessDenied = 4,
        ServiceMissing = 5
    };

    explicit MusicEngine(QObject* parent = nullptr);

    int playbackState() const;
    qint64 position() const;
    qint64 duration() const;
    qreal volume() const;
    void setVolume(qreal v);
    bool seekable() const { return true; }
    bool hasAudio() const;
    bool autoPlay() const;
    void setAutoPlay(bool b);
    int status() const;
    int error() const;
    QString source() const;
    void setSource(const QString& src);
    QString songName() const;
    QString singer() const;
    QString albumName() const;
    QString coverPath() const;

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seek(qint64 ms);
    Q_INVOKABLE void playIndex(int index);

signals:
    void playbackStateChanged(int state);
    void positionChanged(qint64 ms);
    void durationChanged(qint64 ms);
    void volumeChanged(qreal volume);
    void hasAudioChanged(bool has);
    void autoPlayChanged(bool autoplay);
    void statusChanged(int status);
    void errorChanged(int error);
    void sourceChanged(const QString& source);
    void songChanged();
    void coverPathChanged();
    void lyricReady(const QString& lrcText);
    void errorMessage(const QString& msg);

private slots:
    void onUrlReady(const QString& url);
    void onUrlError(const QString& msg);
    void onLyricReadyResult(const QString& lrc);
    void onCoverReady(const QString& songId, const QByteArray& data);
    void onPlayerStateChanged(PlayerState state);
    void onPlayerPosition(qint64 ms);
    void onPlayerDuration(qint64 ms);
    void onPlayerError(const QString& msg);

private:
    int songIndexForId(const QString& id) const;
    void loadCurrentSong();
    void requestLyricAndCover();
    void setPlaybackState(int st);
    void setStatus(int st);

    AlsaPlayer* m_player;
    MusicApi* m_api;
    QList<Song> m_songs;
    Song m_current;
    QString m_source;
    QString m_coverPath;
    int m_playbackState = StoppedState;
    int m_status = NoMedia;
    int m_error = NoError;
    bool m_hasFile = false;
    bool m_autoPlay = false;
    bool m_switching = false;
    bool m_pendingPlay = false;
    int m_vol100 = 80;
};

#endif // MUSICENGINE_H