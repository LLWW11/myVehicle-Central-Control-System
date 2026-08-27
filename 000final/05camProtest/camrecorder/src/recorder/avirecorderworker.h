#ifndef AVIRECORDERWORKER_H
#define AVIRECORDERWORKER_H

#include <QAtomicInt>
#include <QString>
#include <QThread>
#include "jpegframestore.h"
class JpegFrameStore;

// 录像线程
class AviRecorderWorker : public QThread
{
    Q_OBJECT

public:
    AviRecorderWorker(JpegFrameStore *frameStore, const QString &partPath,
                      const QString &finalPath, QObject *parent = nullptr);
    ~AviRecorderWorker() override;

    void requestStop(); // 请求停止录像并唤醒正在等待新帧的线程

signals:

    void recordingStarted(); // avi文件建立信号
    void recordingFinished(bool success, const QString &path,
                           const QString &message); // 录像完成的信号函数

protected:
    /**
     * @brief 执行降帧取样、JPEG 编码、AVI 写入和最终改名
     */
    void run() override;

private:
    JpegFrameStore *m_jpegFrameStore = nullptr;
    QString m_partPath;
    QString m_finalPath;
    QAtomicInt m_stopRequested{0};
};

#endif // AVIRECORDERWORKER_H
