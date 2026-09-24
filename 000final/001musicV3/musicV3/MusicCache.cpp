#include "MusicCache.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

namespace
{
    // 用固定长度散列避免歌曲 ID 进入文件名造成路径问题
    QString key(const QString &source, const QString &songId,
                const QString &quality = QString())
    {
        QByteArray data = source.toUtf8();
        data.append('\0');
        data.append(songId.toUtf8());
        data.append('\0');
        data.append(quality.toUtf8());
        return QString::fromLatin1(
            QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    }

    // 只有确实含有内容的缓存文件才算有效
    bool nonEmptyFile(const QString &path)
    {
        const QFileInfo info(path);
        return !path.isEmpty() && info.isFile() && info.size() > 0;
    }

    // 原子写入缓存文件，失败时不会产生完整文件名
    bool writeFile(const QString &path, const QByteArray &data)
    {
        if (path.isEmpty() || data.isEmpty())
            return false;
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        if (file.write(data) != data.size())
            return false;
        return file.commit();
    }
}

namespace MusicCache
{
    // 创建并返回与 V2 隔离的 musicV3 缓存目录
    QString dir()
    {
        QString root = QStandardPaths::writableLocation(
            QStandardPaths::CacheLocation);
        // 板上未提供 HOME/XDG_CACHE_HOME 时回退到应用目录，仍与 V2 的 cache 隔离。
        if (root.isEmpty())
            root = QCoreApplication::applicationDirPath() + QStringLiteral("/cache");
        if (root.isEmpty())
            return QString();
        const QString path = QDir(root).filePath(QStringLiteral("musicV3"));
        return QDir().mkpath(path) ? path : QString();
    }

    // 生成稳定的 MP3 文件名
    QString mp3File(const QString &source, const QString &songId,
                    const QString &quality)
    {
        const QString root = dir();
        return root.isEmpty() ? QString()
                              : QDir(root).filePath(key(source, songId, quality) + QStringLiteral(".mp3"));
    }

    // 检查 MP3 缓存已提交且非空
    bool hasMp3(const QString &source, const QString &songId,
                const QString &quality)
    {
        return nonEmptyFile(mp3File(source, songId, quality));
    }

    // 生成封面 JPEG 缓存路径
    QString coverFile(const QString &source, const QString &songId)
    {
        const QString root = dir();
        return root.isEmpty() ? QString()
                              : QDir(root).filePath(key(source, songId) + QStringLiteral(".jpg"));
    }

    // 把现有封面路径转为 QML 本地 URL
    QString coverUrl(const QString &source, const QString &songId)
    {
        const QString path = coverFile(source, songId);
        return nonEmptyFile(path) ? QUrl::fromLocalFile(path).toString()
                                  : QString();
    }

    // 检查封面缓存
    bool hasCover(const QString &source, const QString &songId)
    {
        return nonEmptyFile(coverFile(source, songId));
    }

    // 保存 JPEG 封面
    bool saveCover(const QString &source, const QString &songId,
                   const QByteArray &data)
    {
        return writeFile(coverFile(source, songId), data);
    }

    // 生成歌词缓存路径
    QString lrcFile(const QString &source, const QString &songId)
    {
        const QString root = dir();
        return root.isEmpty() ? QString()
                              : QDir(root).filePath(key(source, songId) + QStringLiteral(".lrc"));
    }

    // 从非空缓存读取 UTF-8 歌词
    QString loadLrc(const QString &source, const QString &songId)
    {
        const QString path = lrcFile(source, songId);
        if (!nonEmptyFile(path))
            return QString();
        QFile file(path);
        return file.open(QIODevice::ReadOnly)
                   ? QString::fromUtf8(file.readAll())
                   : QString();
    }

    // 原子保存 UTF-8 歌词
    bool saveLrc(const QString &source, const QString &songId,
                 const QString &text)
    {
        return writeFile(lrcFile(source, songId), text.toUtf8());
    }
}
