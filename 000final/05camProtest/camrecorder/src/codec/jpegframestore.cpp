#include "jpegframestore.h"

#include <QMutexLocker>

quint64 JpegFrameStore::publish(const QByteArray &jpeg,
                                int width,
                                int height)
{
    QMutexLocker locker(&m_mutex);

    ++m_generation;

    m_latestFrame.bytes = jpeg;
    m_latestFrame.width = width;
    m_latestFrame.height = height;
    m_latestFrame.generation = m_generation;

    m_frameArrived.wakeAll();

    return m_generation;
}

JpegFrame JpegFrameStore::latest() const
{
    QMutexLocker locker(&m_mutex);
    return m_latestFrame; // 加锁，拷贝最新一帧
}

bool JpegFrameStore::waitForNewFrame(
    quint64 previousGeneration,
    int timeoutMs,
    JpegFrame *frame)
{
    if (frame == nullptr)
        return false;

    QMutexLocker locker(&m_mutex);

    if (m_latestFrame.generation <= previousGeneration)
    {
        m_frameArrived.wait(
            &m_mutex,
            static_cast<unsigned long>(timeoutMs));
    }

    if (m_latestFrame.generation <= previousGeneration)
        return false;

    *frame = m_latestFrame;

    return frame->isValid();
}

void JpegFrameStore::clear()
{
    QMutexLocker locker(&m_mutex);

    ++m_generation;

    m_latestFrame = JpegFrame();
    m_latestFrame.generation = m_generation;

    m_frameArrived.wakeAll();
}

void JpegFrameStore::wakeAll()
{
    QMutexLocker locker(&m_mutex);
    m_frameArrived.wakeAll();
}