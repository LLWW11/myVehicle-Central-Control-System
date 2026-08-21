#ifndef FRAMESTORE_H
#define FRAMESTORE_H

#include <QByteArray>
#include <QImage>
#include <QMutex>
#include <QWaitCondition>

/**
 * @brief 表示一帧已经按行排列的 RGB565 摄像头数据。
 */
struct CameraFrame
{
    QByteArray bytes;
    int width = 0;
    int height = 0;
    int bytesPerLine = 0;
    quint64 generation = 0;

    /**
     * @brief 判断当前帧是否包含完整、可读取的数据。
     * @return 数据和尺寸均有效时返回 true。
     */
    bool isValid() const;

    /**
     * @brief 将 RGB565 数据转换为拥有独立内存的 QImage。
     * @return 转换后的图像；数据无效时返回空图像。
     */
    QImage toImage() const;
};

/**
 * @brief 在线程之间保存最新摄像头帧，并为录像线程提供条件等待。
 *
 * 该类只保存最新一帧，消费者过慢时会跳过旧帧，防止内存不断增长。
 */
class FrameStore
{
public:
    /**
     * @brief 构造空的帧存储区。
     */
    FrameStore();

    /**
     * @brief 发布一帧新的 RGB565 数据。
     * @param bytes 帧数据，必须至少包含 bytesPerLine * height 字节。
     * @param width 图像宽度。
     * @param height 图像高度。
     * @param bytesPerLine 每行实际字节数。
     * @return 新帧的递增版本号。
     */
    quint64 publish(const QByteArray &bytes, int width, int height,
                    int bytesPerLine);

    /**
     * @brief 获取最新帧的线程安全副本。
     * @return 最新帧；尚未采集到图像时返回无效帧。
     */
    CameraFrame latest() const;

    /**
     * @brief 等待版本号大于指定值的新帧。
     * @param previousGeneration 消费者上次读取的版本号。
     * @param timeoutMs 最长等待时间，单位毫秒。
     * @param frame 用于接收新帧副本。
     * @return 获得新帧时返回 true，超时或仅被唤醒时返回 false。
     */
    bool waitForNewFrame(quint64 previousGeneration, int timeoutMs,
                         CameraFrame *frame);

    /**
     * @brief 清空已有帧并唤醒全部等待线程。
     */
    void clear();

    /**
     * @brief 唤醒等待新帧的线程，供停止录像时快速退出。
     */
    void wakeAll();

private:
    mutable QMutex m_mutex;
    QWaitCondition m_frameArrived;
    CameraFrame m_latestFrame;
    quint64 m_generation = 0;
};

#endif // FRAMESTORE_H

