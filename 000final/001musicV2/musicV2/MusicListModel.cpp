#include "MusicListModel.h"

#include <QRandomGenerator>

#include "MusicCache.h"

MusicListModel::MusicListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int MusicListModel::currentIndex() const
{
    return m_currentIndex;
}

void MusicListModel::setCurrentIndex(int i)
{
    if (i == m_currentIndex)
        return;
    m_currentIndex = i;
    emit currentIndexChanged();
    if (i >= 0 && i < m_songs.size() && m_songName != m_songs.at(i).name) {
        m_songName = m_songs.at(i).name;
        emit songNameChanged();
    }
}

QString MusicListModel::songName() const
{
    return m_songName;
}

int MusicListModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return m_songs.size();
}

QVariant MusicListModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= m_songs.size())
        return QVariant();
    const Song& song = m_songs.at(index.row());
    switch (role) {
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
    endResetModel();
    setCurrentIndex(0);
}

int MusicListModel::randomIndex()
{
    if (m_songs.isEmpty())
        return 0;
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
    return MusicCache::coverUrl(m_songs.at(index).songmid);
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