#include "MusicCache.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>

namespace MusicCache {

static QString baseDir()
{
    return QCoreApplication::applicationDirPath() + QLatin1String("/cache");
}

static QString ensureDir()
{
    const QString path = baseDir();
    QDir().mkpath(path);
    return path;
}

QString dir()
{
    return ensureDir();
}

QString mp3File(const QString& songId)
{
    return ensureDir() + QLatin1Char('/') + songId + QLatin1String(".mp3");
}

bool hasMp3(const QString& songId)
{
    return QFile::exists(mp3File(songId));
}

QString coverFile(const QString& songId)
{
    return ensureDir() + QLatin1Char('/') + songId + QLatin1String(".jpg");
}

QString coverUrl(const QString& songId)
{
    const QString file = coverFile(songId);
    return QFile::exists(file) ? QLatin1String("file://") + file : QString();
}

bool hasCover(const QString& songId)
{
    return QFile::exists(coverFile(songId));
}

void saveCover(const QString& songId, const QByteArray& data)
{
    QFile f(coverFile(songId));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(data);
}

QString lrcFile(const QString& songId)
{
    return ensureDir() + QLatin1Char('/') + songId + QLatin1String(".lrc");
}

bool hasLrc(const QString& songId)
{
    return QFile::exists(lrcFile(songId));
}

QString loadLrc(const QString& songId)
{
    QFile f(lrcFile(songId));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(f.readAll());
}

void saveLrc(const QString& songId, const QString& text)
{
    QFile f(lrcFile(songId));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        f.write(text.toUtf8());
}

} // namespace MusicCache