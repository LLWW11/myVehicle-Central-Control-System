#include "MusicLyricModel.h"

MusicLyricModel::MusicLyricModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int MusicLyricModel::currentIndex() const
{
    return m_currentIndex;
}

int MusicLyricModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return m_lines.size();
}

QVariant MusicLyricModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= m_lines.size())
        return QVariant();
    const LyricLine& line = m_lines.at(index.row());
    switch (role) {
    case timeRole:
        return static_cast<qint64>(line.time);
    case textRole:
        return line.text;
    default:
        return QVariant();
    }
}

void MusicLyricModel::setLrc(const QString& lrcText)
{
    QList<LyricLine> lines = LyricParser::parseLrc(lrcText);
    if (lines.isEmpty()) {
        LyricLine fallback;
        fallback.time = 0;
        fallback.text = QStringLiteral("未找到歌词");
        lines.append(fallback);
    }

    beginResetModel();
    m_lines = lines;
    endResetModel();
    setcurrentIndex(0);
}

int MusicLyricModel::findIndex(int position)
{
    int lo = 0, hi = m_lines.size() - 1, ans = 0;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (m_lines.at(mid).time <= position) {
            ans = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    setcurrentIndex(ans);
    return ans;
}

int MusicLyricModel::getIndex(int position)
{
    return findIndex(position);
}

void MusicLyricModel::setcurrentIndex(int i)
{
    if (m_lines.isEmpty())
        return;
    if (i < 0)
        i = 0;
    if (i >= m_lines.size())
        i = m_lines.size() - 1;
    if (m_currentIndex == i)
        return;
    m_currentIndex = i;
    Q_EMIT currentIndexChanged();
}

QHash<int, QByteArray> MusicLyricModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[timeRole] = "time";
    roles[textRole] = "textLine";
    return roles;
}