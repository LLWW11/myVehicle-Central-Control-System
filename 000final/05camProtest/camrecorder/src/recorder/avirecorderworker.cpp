#include "avirecorderworker.h"

#include "aviwriter.h"
#include "jpegframestore.h"

#include <QFile>

namespace
{
    constexpr int kRecordFps = 15;
    // 编码质量由编码线程JpegEncoderWorker 决定
    //  constexpr int kJpegQuality = 60;
    //  constexpr int kCaptureToRecordDivider = 2;
}

AviRecorderWorker::AviRecorderWorker(JpegFrameStore *jpegFrameStore,
                                     const QString &partPath,
                                     const QString &finalPath,
                                     QObject *parent)
    : QThread(parent),
      m_jpegFrameStore(jpegFrameStore),
      m_partPath(partPath),
      m_finalPath(finalPath)
{
}

AviRecorderWorker::~AviRecorderWorker()
{
    requestStop();
    wait(5000);
}
void AviRecorderWorker::requestStop()
{
    m_stopRequested.storeRelease(1);
    if (m_jpegFrameStore != nullptr)
        m_jpegFrameStore->wakeAll();
}

void AviRecorderWorker::run()
{
    // 每段录像使用一个新线程对象，保留启动瞬间可能已经发出的停止请求
    if (m_stopRequested.loadAcquire() != 0)
    {
        emit recordingFinished(false, m_partPath,
                               QStringLiteral("录像在启动前已被停止"));
        return;
    }

    AviWriter writer;
    // 这里部分参数仅记录进AVI头部，实际质量由编码线程决定
    if (!writer.open(m_partPath, 640, 480, kRecordFps, 85))
    {
        emit recordingFinished(false, m_partPath, writer.errorString());
        return;
    }
    emit recordingStarted();

    quint64 previousGeneration = 0;
    QString failure;

    while (m_stopRequested.loadAcquire() == 0)
    {
        JpegFrame frame;
        if (!m_jpegFrameStore->waitForNewFrame(previousGeneration, 100, &frame))
        {
            // 帧无效（如 clear 之后 generation 领先）时 waitForNewFrame 会立即返回，
            // 此处必须同步推进 generation，否则下面变成 100% CPU 忙转死循环。
            if (frame.generation > previousGeneration)
                previousGeneration = frame.generation;
            continue;
        }
        previousGeneration = frame.generation;

        // JPEG 流本身已是约 15 FPS，来一帧写一帧即可
        if (!writer.appendJpegFrame(frame.bytes))
        {
            failure = writer.errorString();
            break;
        }
    }

    // 无论哪条路径退出循环，都必须完成收尾并发出 recordingFinished，
    // 否则界面状态机停留在 Stopping 无法恢复（表现为点击停止后卡死）。
    if (!failure.isEmpty())
    {
        writer.abort();
        emit recordingFinished(false, m_partPath, failure);
        return;
    }
    if (!writer.finalize())
    {
        emit recordingFinished(false, m_partPath, writer.errorString());
        return;
    }

    if (QFile::exists(m_finalPath) || !QFile::rename(m_partPath, m_finalPath))
    {
        emit recordingFinished(false, m_partPath,
                               QStringLiteral("AVI 已收尾，但临时文件改名失败"));
        return;
    }
    emit recordingFinished(true, m_finalPath,
                           QStringLiteral("录像已保存，共 %1 帧")
                               .arg(writer.frameCount()));
}
