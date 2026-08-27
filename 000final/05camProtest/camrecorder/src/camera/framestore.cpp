#include "framestore.h"

#include <QMutexLocker>

bool CameraFrame::isValid() const
{
    const qint64 requiredSize = static_cast<qint64>(bytesPerLine) * height;
    return width > 0 && height > 0 && bytesPerLine >= width * 2 && requiredSize > 0 && bytes.size() >= requiredSize;
}

QImage CameraFrame::toImage() const
{
    if (!isValid())
        return QImage();

    const QImage wrapped(reinterpret_cast<const uchar *>(bytes.constData()),
                         width, height, bytesPerLine, QImage::Format_RGB16);
    return wrapped.copy();
}

FrameStore::FrameStore() = default;

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

CameraFrame FrameStore::latest() const
{
    QMutexLocker locker(&m_mutex);
    return m_latestFrame;
}

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

void FrameStore::clear()
{
    QMutexLocker locker(&m_mutex);
    ++m_generation;
    m_latestFrame = CameraFrame();
    m_latestFrame.generation = m_generation;
    m_frameArrived.wakeAll();
}

void FrameStore::wakeAll()
{
    QMutexLocker locker(&m_mutex);
    m_frameArrived.wakeAll();
}
