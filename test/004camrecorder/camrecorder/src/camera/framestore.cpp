#include "framestore.h"

#include <QMutexLocker>

/**
 * @brief 判断当前帧是否包含完整、可读取的数据。
 */
bool CameraFrame::isValid() const
{
    const qint64 requiredSize = static_cast<qint64>(bytesPerLine) * height;
    return width > 0 && height > 0 && bytesPerLine >= width * 2
            && requiredSize > 0 && bytes.size() >= requiredSize;
}

/**
 * @brief 将 RGB565 数据转换为拥有独立内存的 QImage。
 */
QImage CameraFrame::toImage() const
{
    if (!isValid())
        return QImage();

    // QImage::copy() 使图像不再依赖 CameraFrame 中 QByteArray 的生命周期。
    const QImage wrapped(reinterpret_cast<const uchar *>(bytes.constData()),
                         width, height, bytesPerLine, QImage::Format_RGB16);
    return wrapped.copy();
}

/**
 * @brief 构造空的帧存储区。
 */
FrameStore::FrameStore() = default;

/**
 * @brief 发布一帧新的 RGB565 数据。
 */
quint64 FrameStore::publish(const QByteArray &bytes, int width, int height,
                            int bytesPerLine)
{
    QMutexLocker locker(&m_mutex);
    ++m_generation;
    m_latestFrame.bytes = bytes;
    m_latestFrame.width = width;
    m_latestFrame.height = height;
    m_latestFrame.bytesPerLine = bytesPerLine;
    m_latestFrame.generation = m_generation;
    m_frameArrived.wakeAll();
    return m_generation;
}

/**
 * @brief 获取最新帧的线程安全副本。
 */
CameraFrame FrameStore::latest() const
{
    QMutexLocker locker(&m_mutex);
    return m_latestFrame;
}

/**
 * @brief 等待版本号大于指定值的新帧。
 */
bool FrameStore::waitForNewFrame(quint64 previousGeneration, int timeoutMs,
                                 CameraFrame *frame)
{
    if (frame == nullptr)
        return false;

    QMutexLocker locker(&m_mutex);
    if (m_latestFrame.generation <= previousGeneration)
        m_frameArrived.wait(&m_mutex, static_cast<unsigned long>(timeoutMs));

    if (m_latestFrame.generation <= previousGeneration)
        return false;

    *frame = m_latestFrame;
    return frame->isValid();
}

/**
 * @brief 清空已有帧并唤醒全部等待线程。
 */
void FrameStore::clear()
{
    QMutexLocker locker(&m_mutex);
    ++m_generation;
    m_latestFrame = CameraFrame();
    m_latestFrame.generation = m_generation;
    m_frameArrived.wakeAll();
}

/**
 * @brief 唤醒等待新帧的线程，供停止录像时快速退出。
 */
void FrameStore::wakeAll()
{
    QMutexLocker locker(&m_mutex);
    m_frameArrived.wakeAll();
}

