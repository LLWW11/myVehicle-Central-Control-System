#ifndef AVIWRITER_H
#define AVIWRITER_H

#include <QByteArray>
#include <QString>
#include <QVector>

#include <cstdio>

struct CameraFrame;

/**
 * @brief 将 RGB565 帧压缩为 JPEG 并写入带索引的 MJPEG AVI 文件。
 *
 * 每个 AviWriter 实例只对应一次录像，不使用全局状态，可连续创建多段录像。
 */
class AviWriter
{
public:
    /**
     * @brief 构造未打开文件的 AVI 写入器。
     */
    AviWriter();

    /**
     * @brief 析构写入器；若尚未正常完成，只关闭文件而不伪造成功收尾。
     */
    ~AviWriter();

    /**
     * @brief 创建 AVI 临时文件并写入初始文件头。
     * @param partPath 录像期间使用的 .avi.part 路径。
     * @param width 视频宽度。
     * @param height 视频高度。
     * @param fps AVI 标称帧率。
     * @param jpegQuality JPEG 压缩质量。
     * @return 成功返回 true，失败原因通过 errorString() 获取。
     */
    bool open(const QString &partPath, int width, int height,
              int fps, int jpegQuality);

    /**
     * @brief 将一帧 RGB565 数据压缩并写入 AVI。
     * @param frame 摄像头帧。
     * @return 写入成功返回 true。
     */
    bool appendFrame(const CameraFrame &frame);

    /**
     * @brief 写入 idx1 索引、回填文件头并同步关闭文件。
     * @return 收尾完整时返回 true。
     */
    bool finalize();

    /**
     * @brief 放弃当前录像并关闭文件，不写入成功收尾。
     */
    void abort();

    /**
     * @brief 获取最近一次失败原因。
     * @return 可显示的中文错误信息。
     */
    QString errorString() const;

    /**
     * @brief 获取已经成功写入的帧数。
     * @return 已写帧数。
     */
    quint32 frameCount() const;

private:
    /**
     * @brief 描述一帧在 movi 数据块中的索引信息。
     */
    struct IndexEntry
    {
        quint32 offset = 0;
        quint32 size = 0;
    };

    /**
     * @brief 根据当前视频参数构造 232 字节 AVI 头。
     * @return 初始文件头。
     */
    QByteArray buildHeader() const;

    /**
     * @brief 将 RGB565 摄像头帧压缩为内存中的 JPEG。
     * @param frame 输入帧。
     * @param jpeg 接收压缩结果。
     * @return 压缩成功返回 true。
     */
    bool encodeJpeg(const CameraFrame &frame, QByteArray *jpeg);

    /**
     * @brief 写入精确数量的字节并统一记录错误。
     * @param data 数据地址。
     * @param size 字节数。
     * @return 完整写入返回 true。
     */
    bool writeBytes(const void *data, size_t size);

    /**
     * @brief 跳转到指定文件偏移并写入一个小端 32 位整数。
     * @param offset 文件起始位置的偏移量。
     * @param value 待写值。
     * @return 操作成功返回 true。
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

