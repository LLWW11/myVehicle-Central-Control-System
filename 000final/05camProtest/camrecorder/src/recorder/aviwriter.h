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

    // 一帧 RGB565 数据压缩并写入 AVI
    bool appendFrame(const CameraFrame &frame);

    // 将一段已经编码好的 JPEG 帧写入 AVI
    bool appendJpegFrame(const QByteArray &jpeg);

    /**
     * @brief 写入 idx1 索引、回填文件头并同步关闭文件
     * @return 收尾完整时返回 true
     */
    bool finalize();

    /**
     * @brief 放弃当前录像并关闭文件，不写入成功收尾
     */
    void abort();

    /**
     * @brief 获取最近一次失败原因
     * @return 可显示的中文错误信息
     */
    QString errorString() const;

    /**
     * @brief 获取已经成功写入的帧数
     * @return 已写帧数
     */
    quint32 frameCount() const;

private:
    /**
     * @brief 描述一帧在 movi 数据块中的索引信息
     */
    struct IndexEntry
    {
        quint32 offset = 0;
        quint32 size = 0;
    };

    /**
     * @brief 根据当前视频参数构造 232 字节 AVI 头
     * @return 初始文件头
     */
    QByteArray buildHeader() const;

    /**
     * @brief 写入精确数量的字节并统一记录错误
     * @param data 数据地址
     * @param size 字节数
     * @return 完整写入返回 true
     */
    bool writeBytes(const void *data, size_t size);

    /**
     * @brief 跳转到指定文件偏移并写入一个小端 32 位整数
     * @param offset 文件起始位置的偏移量
     * @param value 待写值
     * @return 操作成功返回 true
     */
    bool patchLe32(long offset, quint32 value);

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
