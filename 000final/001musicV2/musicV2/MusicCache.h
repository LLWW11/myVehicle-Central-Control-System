#ifndef MUSICCACHE_H
#define MUSICCACHE_H

#include <QByteArray>
#include <QString>

/*
 * 所有从网络下载的内容(MP3 音频 / 封面图像 / 歌词元数据)
 * 统一保存到 <applicationDir>/cache/ 目录(即部署目录下的 ui/cache)。
 * 缓存文件名统一使用歌曲 ID 区分: <id>.mp3 / <id>.jpg / <id>.lrc。
 */
namespace MusicCache {

QString dir();
QString mp3File(const QString& songId);
bool hasMp3(const QString& songId);
QString coverFile(const QString& songId);
QString coverUrl(const QString& songId);
bool hasCover(const QString& songId);
void saveCover(const QString& songId, const QByteArray& data);
QString lrcFile(const QString& songId);
bool hasLrc(const QString& songId);
QString loadLrc(const QString& songId);
void saveLrc(const QString& songId, const QString& text);

}

#endif // MUSICCACHE_H