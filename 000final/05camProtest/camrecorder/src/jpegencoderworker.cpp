#include "jpegencoderworker.h"

#include "camera/framestore.h"
#include "jpegencoder.h"
#include "jpegframestore.h"

namespace
{

    constexpr int kJpegQuality = 80;
    constexpr int kCaptureDivider = 2;

}

JpegEncoderWorker::JpegEncoderWorker(FrameStore *rawStore,
                                     JpegFrameStore *jpegStore,
                                     QObject *parent)
    : QThread(parent), m_rawStore(rawStore), m_jpegStore(jpegStore)
{
}

JpegEncoderWorker::~JpegEncoderWorker()
{
    requestStop();
    wait(5000);
}

void JpegEncoderWorker::requestStop()
{
    m_stopRequested.storeRelease(1);

    if (m_rawStore != nullptr)
        m_rawStore->wakeAll();
}

void JpegEncoderWorker::run()
{
    quint64 previousGeneration = 0;
    quint64 capturedFrames = 0;

    while (m_stopRequested.loadAcquire() == 0)
    {
        if (m_rawStore == nullptr || m_jpegStore == nullptr)
            break;

        CameraFrame frame;

        if (!m_rawStore->waitForNewFrame(
                previousGeneration,
                100,
                &frame))
        {
            // 帧无效（比如 clear 之后 generation 领先）时 waitForNewFrame 会立即返回，
            // 此处必须同步推进 generation，否则变成 100% CPU 忙转死循环
            if (frame.generation > previousGeneration)
                previousGeneration = frame.generation;
            continue;
        }

        previousGeneration = frame.generation;

        ++capturedFrames;

        if ((capturedFrames % kCaptureDivider) != 0)
            continue;

        QByteArray jpeg;
        QString error;

        if (!JpegEncoder::encode(
                frame,
                kJpegQuality,
                &jpeg,
                &error))
        {

            Q_EMIT encoderError(error);
            continue;
        }

        m_jpegStore->publish(
            jpeg,
            frame.width,
            frame.height);
    }
}