#ifndef CAMERACAPTURE_H
#define CAMERACAPTURE_H

#include <QAtomicInt>
#include <QString>
#include <QThread>
#include <QVector>

#include <cstddef>

class FrameStore;

class CameraCapture : public QThread // 采集 RGB565 图像线程
{
    Q_OBJECT

public:
    explicit CameraCapture(FrameStore *frameStore, QObject *parent = nullptr);
    ~CameraCapture() override;
    void requestStop(); // 停止该线程

signals:

    void captureStarted(int width, int height, int bytesPerLine); // 开始采集的信号
    void captureError(const QString &message);
    void captureStopped();

protected:
    void run() override; // V4L2 初始化、采集循环和统一清理

private:
    struct MappedBuffer
    {
        void *address = nullptr;
        size_t length = 0;
    };
    bool openDevice();
    bool configureDevice();                 // 固定 RGB565、640×480、30 FPS 格式
    bool mapBuffers();                      // 申请并映射 V4L2 缓冲区
    bool startStreaming();                  //
    bool captureOneFrame(bool *firstFrame); // 采集一帧并发布到 FrameStore
    void cleanup();
    void reportSystemError(const QString &operation); // 记录错误operation并发送captureError 信号

    FrameStore *m_frameStore = nullptr;
    QAtomicInt m_stopRequested{0};
    int m_fd = -1;
    bool m_streaming = false;
    int m_width = 0;
    int m_height = 0;
    int m_bytesPerLine = 0;
    QVector<MappedBuffer> m_buffers;
};

#endif // CAMERACAPTURE_H
