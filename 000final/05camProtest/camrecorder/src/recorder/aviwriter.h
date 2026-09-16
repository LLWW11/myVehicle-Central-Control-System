#ifndef AVIWRITER_H
#define AVIWRITER_H

#include <QByteArray>
#include <QString>
#include <QVector>

#include <cstdio>

struct CameraFrame;

class AviWriter
{
public:
    AviWriter();
    ~AviWriter();

    // AVI临时文件.avi.part写入文件头
    bool open(const QString &partPath,
              int width, int height,
              int fps, // 视频的帧率15fps
              int jpegQuality);

    // 弃用：一帧 RGB565 数据压缩并写入 AVI
    // 现在预览也是使用JPEG帧变成 QImage
    bool appendFrame(const CameraFrame &frame);

    // 将一段已经编码好的 JPEG 帧写入 AVI
    bool appendJpegFrame(const QByteArray &jpeg);
    // 收尾或者丢弃
    bool finalize();
    void abort();

    QString errorString() const;
    quint32 frameCount() const; // 获取已经成功写入的帧数

private:
    struct IndexEntry // 描述一帧在 movi 数据块中的索引信息
    {
        quint32 offset = 0;
        quint32 size = 0;
    };
    QByteArray buildHeader() const;

    bool writeBytes(const void *data, size_t size); // 向data地址写入字节

    bool patchLe32(long offset, quint32 value); // offset上写一个小端int32
    FILE *m_file = nullptr;
    QString m_partPath;
    QString m_errorString;
    int m_width = 0;
    int m_height = 0;
    int m_fps = 0;
    int m_jpegQuality = 0;
    quint32 m_frames = 0;
    quint32 m_maxFrameSize = 0;
    quint32 m_chunkOffset = 4;
    QVector<IndexEntry> m_index;
};

#endif // AVIWRITER_H
