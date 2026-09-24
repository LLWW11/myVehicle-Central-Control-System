#ifndef MUSICCACHE_H
#define MUSICCACHE_H

#include <QByteArray>
#include <QString>

/** @brief musicV3 的本地 MP3、歌词和封面缓存路径与原子写入。 */
namespace MusicCache {
/** @brief 返回或创建应用专属缓存目录。 */
QString dir();
/** @brief 返回包含音源、歌曲 ID 和音质的 MP3 路径。 */
QString mp3File(const QString &source, const QString &songId,
                const QString &quality);
/** @brief 仅把非空文件视为有效 MP3 缓存。 */
bool hasMp3(const QString &source, const QString &songId,
            const QString &quality);
/** @brief 返回音源与歌曲 ID 对应的封面文件路径。 */
QString coverFile(const QString &source, const QString &songId);
/** @brief 返回 QML 可读取的本地封面 URL。 */
QString coverUrl(const QString &source, const QString &songId);
/** @brief 检查非空封面文件是否存在。 */
bool hasCover(const QString &source, const QString &songId);
/** @brief 原子保存已转换为 JPEG 的封面。 */
bool saveCover(const QString &source, const QString &songId,
               const QByteArray &data);
/** @brief 返回音源与歌曲 ID 对应的歌词文件路径。 */
QString lrcFile(const QString &source, const QString &songId);
/** @brief 从缓存读取歌词，不存在时返回空文本。 */
QString loadLrc(const QString &source, const QString &songId);
/** @brief 原子保存歌词文本。 */
bool saveLrc(const QString &source, const QString &songId,
             const QString &text);
}

#endif // MUSICCACHE_H
