#ifndef CAMERACAPTURE_H
#define CAMERACAPTURE_H

#include <QAtomicInt>
#include <QString>
#include <QThread>
#include <QVector>

#include <cstddef>

class FrameStore;

/**
 * @brief 使用 V4L2 MMAP 从固定摄像头节点采集 RGB565 图像的工作线程
 */
class CameraCapture : public QThread
{
    Q_OBJECT

public:
    /**
     * @brief 构造摄像头采集线程。
     * @param frameStore 最新帧存储区，不转移所有权。
     * @param parent Qt 父对象。
     */
    explicit CameraCapture(FrameStore *frameStore, QObject *parent = nullptr);

    ~CameraCapture() override;

    /**
     * @brief 请求采集线程停止。
     *
     * 该函数只设置线程安全标志；调用者随后应使用 wait() 等待退出。
     */
    void requestStop();

Q_SIGNALS:
    /**
     * @brief 首次成功采集到一帧后发送。
     * @param width 实际图像宽度。
     * @param height 实际图像高度。
     * @param bytesPerLine 驱动返回的实际行步长。
     */
    void captureStarted(int width, int height, int bytesPerLine);

    /**
     * @brief 摄像头打开或采集失败。
     * @param message 可直接显示给用户的错误信息。
     */
    void captureError(const QString &message);

    /**
     * @brief 采集线程已结束并释放全部 V4L2 资源。
     */
    void captureStopped();

protected:
    /**
     * @brief 执行 V4L2 初始化、采集循环和统一清理。
     */
    void run() override;

private:
    /**
     * @brief 描述一个 V4L2 MMAP 缓冲区。
     */
    struct MappedBuffer
    {
        void *address = nullptr;
        size_t length = 0;
    };

    /**
     * @brief 打开设备并验证其视频采集能力。
     * @return 成功返回 true。
     */
    bool openDevice();

    /**
     * @brief 配置固定的 RGB565、640×480、30 FPS 格式。
     * @return 驱动接受固定格式时返回 true。
     */
    bool configureDevice();

    /**
     * @brief 申请并映射 V4L2 缓冲区，然后将其全部入队。
     * @return 成功返回 true。
     */
    bool mapBuffers();

    /**
     * @brief 开启 V4L2 数据流。
     * @return 成功返回 true。
     */
    bool startStreaming();

    /**
     * @brief 采集一帧并发布到 FrameStore。
     * @param firstFrame 是否尚未发送 captureStarted 信号。
     * @return 可继续采集时返回 true；致命错误时返回 false。
     */
    bool captureOneFrame(bool *firstFrame);

    /**
     * @brief 停止数据流、解除映射并关闭设备。
     */
    void cleanup();

    /**
     * @brief 记录最近错误并发送 captureError 信号。
     * @param operation 失败的操作名称。
     */
    void reportSystemError(const QString &operation);

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
