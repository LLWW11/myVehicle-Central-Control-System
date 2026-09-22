#ifndef SONG_H
#define SONG_H

#include <QString>

/*
 * 歌曲数据结构(与 000CMu14 原工程一致)。
 * songId 来自网易云公开接口(music.163.com/api/song/detail),
 * 播放 URL 由音源 API(source.shiqianjiang.cn)动态解析, 代码里不存死链接。
 */
struct Song {
    QString songmid;    // 歌曲 ID (网易为数字 id)
    QString name;       // 歌名
    QString singer;     // 歌手
    QString albumName;  // 专辑
    QString source;     // 音源: "wy" / "kw" / "tx" / "kg" / "mg" / "git"
    int     duration;   // 时长(秒), 0 表示未知
    QString img;        // 封面 URL(未用到可留空)
    QString quality;    // 音质, 默认 "320k"
};

#endif // SONG_H
