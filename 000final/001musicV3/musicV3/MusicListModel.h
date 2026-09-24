#ifndef MUSICLISTMODEL_H
#define MUSICLISTMODEL_H

#include <QAbstractListModel>
#include <QString>

#include "data/SongData.h"

/*
 * MusicListModel - 预置歌曲列表(QML 中注册名为 PlayListModel)。
 * 提供与旧 playListModel 兼容的接口(title/author 角色, getcurrentPath() 等),
 * 由 presetSongs() 填充, 播放源为歌曲 ID(由 MusicEngine 解析)。
 */
class MusicListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QString songName READ songName NOTIFY songNameChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    explicit MusicListModel(QObject *parent = nullptr);

    int currentIndex() const;
    void setCurrentIndex(int i);
    // 返回当前列表歌曲数
    int count() const;
    QString songName() const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    Q_INVOKABLE void addPresets();
    Q_INVOKABLE int randomIndex();
    Q_INVOKABLE QString getcurrentPath() const;
    Q_INVOKABLE QString getcurrentTitle() const;
    Q_INVOKABLE QString getcurrentAuthor() const;
    Q_INVOKABLE QString getcurrentSongName() const;
    Q_INVOKABLE QString getSongName(int index) const;
    Q_INVOKABLE QString coverUrl(int index) const;
    // 返回循环的下一首索引，空列表返回 -1
    Q_INVOKABLE int nextIndex() const;
    // 返回循环的上一首索引，空列表返回 -1
    Q_INVOKABLE int previousIndex() const;

    enum roleType
    {
        titleRole = Qt::UserRole + 1,
        authorRole,
    };

Q_SIGNALS:
    void currentIndexChanged();
    void songNameChanged();
    void countChanged();

protected:
    QHash<int, QByteArray> roleNames() const override;

private:
    bool validIndex() const;
    QList<Song> m_songs;
    int m_currentIndex = -1;
    QString m_songName;
};

#endif // MUSICLISTMODEL_H
