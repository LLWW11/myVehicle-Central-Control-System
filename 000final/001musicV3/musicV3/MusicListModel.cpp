#include "MusicListModel.h"

#include <QRandomGenerator>

#include "MusicCache.h"

MusicListModel::MusicListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MusicListModel::currentIndex() const
{
    return m_currentIndex;
}

void MusicListModel::setCurrentIndex(int i)
{
    if (i < -1 || i >= m_songs.size() ||
        (i == -1 && !m_songs.isEmpty()))
        return;
    if (i == m_currentIndex)
        return;
    m_currentIndex = i;
    Q_EMIT currentIndexChanged();
    const QString newName = validIndex() ? m_songs.at(i).name : QString();
    if (m_songName != newName)
    {
        m_songName = newName;
        Q_EMIT songNameChanged();
    }
}

// 返回歌曲列表大小
int MusicListModel::count() const
{
    return m_songs.size();
}

QString MusicListModel::songName() const
{
    return m_songName;
}

int MusicListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_songs.size();
}

QVariant MusicListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_songs.size())
        return QVariant();
    const Song &song = m_songs.at(index.row());
    switch (role)
    {
    case titleRole:
        return song.name;
    case authorRole:
        return song.singer;
    default:
        return QVariant();
    }
}

void MusicListModel::addPresets()
{
    beginResetModel();
    m_songs = presetSongs();
    m_currentIndex = -1;
    m_songName.clear();
    endResetModel();
    Q_EMIT countChanged();
    if (!m_songs.isEmpty())
        setCurrentIndex(0);
}

int MusicListModel::randomIndex()
{
    if (m_songs.isEmpty())
        return -1;
    setCurrentIndex(QRandomGenerator::global()->bounded(m_songs.size()));
    return m_currentIndex;
}

QString MusicListModel::getcurrentPath() const
{
    return validIndex() ? m_songs.at(m_currentIndex).songmid : QString();
}

QString MusicListModel::getcurrentTitle() const
{
    return validIndex() ? m_songs.at(m_currentIndex).name : QString();
}

QString MusicListModel::getcurrentAuthor() const
{
    return validIndex() ? m_songs.at(m_currentIndex).singer : QString();
}

QString MusicListModel::getcurrentSongName() const
{
    return validIndex() ? m_songs.at(m_currentIndex).name : QString();
}

QString MusicListModel::getSongName(int index) const
{
    if (index < 0 || index >= m_songs.size())
        return QString();
    return m_songs.at(index).name;
}

QString MusicListModel::coverUrl(int index) const
{
    if (index < 0 || index >= m_songs.size())
        return QString();
    const Song &song = m_songs.at(index);
    return MusicCache::coverUrl(song.source, song.songmid);
}

// 计算循环播放时的下一首索引
int MusicListModel::nextIndex() const
{
    if (m_songs.isEmpty())
        return -1;
    return validIndex() ? (m_currentIndex + 1) % m_songs.size() : 0;
}

// 计算循环播放时的上一首索引
int MusicListModel::previousIndex() const
{
    if (m_songs.isEmpty())
        return -1;
    return validIndex() ? (m_currentIndex + m_songs.size() - 1) % m_songs.size() : 0;
}

QHash<int, QByteArray> MusicListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[titleRole] = "title";
    roles[authorRole] = "author";
    return roles;
}

bool MusicListModel::validIndex() const
{
    return m_currentIndex >= 0 && m_currentIndex < m_songs.size();
}
