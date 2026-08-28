#include "avirecorderworker.h"

#include "aviwriter.h"
#include "camera/framestore.h"

#include <QFile>

namespace {
constexpr int kRecordFps = 15;
constexpr int kJpegQuality = 85;
constexpr int kCaptureToRecordDivider = 2;
}

/**
 * @brief 构造录像线程。
 */
AviRecorderWorker::AviRecorderWorker(FrameStore *frameStore,
                                     const QString &partPath,
                                     const QString &finalPath,
                                     QObject *parent)
    : QThread(parent)
    , m_frameStore(frameStore)
    , m_partPath(partPath)
    , m_finalPath(finalPath)
{
}

/**
 * @brief 析构录像线程并确保线程停止。
 */
AviRecorderWorker::~AviRecorderWorker()
{
    requestStop();
    wait(5000);
}

/**
 * @brief 请求停止录像并唤醒正在等待新帧的线程。
 */
void AviRecorderWorker::requestStop()
{
    m_stopRequested.storeRelease(1);
    if (m_frameStore != nullptr)
        m_frameStore->wakeAll();
}

/**
 * @brief 执行降帧取样、JPEG 编码、AVI 写入和最终改名。
 */
void AviRecorderWorker::run()
{
    // 每段录像使用一个新线程对象，保留启动瞬间可能已经发出的停止请求。
    if (m_stopRequested.loadAcquire() != 0) {
        Q_EMIT recordingFinished(false, m_partPath,
                               QStringLiteral("录像在启动前已被停止"));
        return;
    }

    AviWriter writer;
    if (!writer.open(m_partPath, 640, 480, kRecordFps, kJpegQuality)) {
        Q_EMIT recordingFinished(false, m_partPath, writer.errorString());
        return;
    }

    Q_EMIT recordingStarted();
    quint64 previousGeneration = 0;
    quint64 capturedFrames = 0;
    QString failure;

    while (m_stopRequested.loadAcquire() == 0) {
        CameraFrame frame;
        if (!m_frameStore->waitForNewFrame(previousGeneration, 100, &frame))
            continue;
        previousGeneration = frame.generation;
        ++capturedFrames;

        // 摄像头固定为 30 FPS，每两帧取一帧写入 15 FPS AVI。
        if ((capturedFrames % kCaptureToRecordDivider) != 0)
            continue;
        if (!writer.appendFrame(frame)) {
            failure = writer.errorString();
            break;
        }
    }

    if (!failure.isEmpty()) {
        writer.abort();
        Q_EMIT recordingFinished(false, m_partPath, failure);
        return;
    }
    if (!writer.finalize()) {
        Q_EMIT recordingFinished(false, m_partPath, writer.errorString());
        return;
    }

    if (QFile::exists(m_finalPath) || !QFile::rename(m_partPath, m_finalPath)) {
        Q_EMIT recordingFinished(false, m_partPath,
                               QStringLiteral("AVI 已收尾，但临时文件改名失败"));
        return;
    }
    Q_EMIT recordingFinished(true, m_finalPath,
                           QStringLiteral("录像已保存，共 %1 帧")
                           .arg(writer.frameCount()));
}
