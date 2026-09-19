#ifndef FRAMESTORE_H
#define FRAMESTORE_H

#include <QByteArray>
#include <QImage>
#include <QMutex>
#include <QWaitCondition>

struct CameraFrame // 摄像头一帧数据
{
    QByteArray bytes;
    int width = 0;
    int height = 0;
    int bytesPerLine = 0;
    quint64 generation = 0;

    bool isValid() const;   // 判断当前帧是否包含完整、可读取的数据
    QImage toImage() const; // RGB565数据转换为QImage,无效时返回空
};

class FrameStore // 最新一帧数据，用数组装载，消费者太慢会覆盖
{
public:
    FrameStore();

    quint64 publish(const QByteArray &bytes,
                    int width,
                    int height,
                    int bytesPerLine);
    CameraFrame latest() const;
    // 消费者等待获取最新一帧，获得新帧时返回 true，超时或仅被唤醒时返回 false
    bool waitForNewFrame(quint64 previousGeneration,
                         int timeoutMs,
                         CameraFrame *frame);

    void clear();
    void wakeAll(); // 唤醒消费者线程

private:
    mutable QMutex m_mutex;
    QWaitCondition m_frameArrived; // 条件变量，用于唤醒其他线程
    CameraFrame m_latestFrame;
    quint64 m_generation = 0;
};

#endif // FRAMESTORE_H
