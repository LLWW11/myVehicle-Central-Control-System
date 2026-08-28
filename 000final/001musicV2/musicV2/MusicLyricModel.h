#ifndef MUSICLYRICMODEL_H
#define MUSICLYRICMODEL_H

#include <QAbstractListModel>
#include <QString>

#include "lyrics/LyricParser.h"

/*
 * MusicLyricModel - 歌词列表(QML 中注册名�?LyricModel)�? * �?MusicEngine 拉取网络 LRC 文本�?LyricParser 解析后填�?
 * 提供与旧 lyricModel 一致的 findIndex()/getIndex()/currentIndex 接口�? */
class MusicLyricModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setcurrentIndex NOTIFY currentIndexChanged)
public:
    explicit MusicLyricModel(QObject* parent = nullptr);

    int currentIndex() const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    Q_INVOKABLE void setLrc(const QString& lrcText);
    Q_INVOKABLE int findIndex(int position);
    Q_INVOKABLE int getIndex(int position);
    void setcurrentIndex(int i);

    enum lyricRole {
        timeRole = Qt::UserRole + 1,
        textRole,
    };

Q_SIGNALS:
    void currentIndexChanged();

protected:
    QHash<int, QByteArray> roleNames() const override;

private:
    QList<LyricLine> m_lines;
    int m_currentIndex = 0;
};

#endif // MUSICLYRICMODEL_H