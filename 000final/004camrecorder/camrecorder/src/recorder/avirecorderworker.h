#ifndef AVIRECORDERWORKER_H
#define AVIRECORDERWORKER_H

#include <QAtomicInt>
#include <QString>
#include <QThread>

class FrameStore;

/**
 * @brief 从 FrameStore 取帧并以 15 FPS 写入 MJPEG AVI 的工作线程。
 */
class AviRecorderWorker : public QThread
{
    Q_OBJECT

public:
    /**
     * @brief 构造录像线程。
     * @param frameStore 最新帧存储区，不转移所有权。
     * @param partPath 录像临时文件路径。
     * @param finalPath 成功完成后的 AVI 文件路径。
     * @param parent Qt 父对象。
     */
    AviRecorderWorker(FrameStore *frameStore, const QString &partPath,
                      const QString &finalPath, QObject *parent = nullptr);

    /**
     * @brief 析构录像线程并确保线程停止。
     */
    ~AviRecorderWorker() override;

    /**
     * @brief 请求停止录像并唤醒正在等待新帧的线程。
     */
    void requestStop();

Q_SIGNALS:
    /**
     * @brief AVI 临时文件已建立，可以显示为正在录像。
     */
    void recordingStarted();

    /**
     * @brief 录像线程完成。
     * @param success AVI 已完整收尾并改名时为 true。
     * @param path 成功时为最终 AVI 路径，失败时为 .part 路径。
     * @param message 结果说明或错误原因。
     */
    void recordingFinished(bool success, const QString &path,
                           const QString &message);

protected:
    /**
     * @brief 执行降帧取样、JPEG 编码、AVI 写入和最终改名。
     */
    void run() override;

private:
    FrameStore *m_frameStore = nullptr;
    QString m_partPath;
    QString m_finalPath;
    QAtomicInt m_stopRequested {0};
};

#endif // AVIRECORDERWORKER_H
