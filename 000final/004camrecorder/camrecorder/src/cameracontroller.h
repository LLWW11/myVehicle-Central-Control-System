#ifndef CAMERACONTROLLER_H
#define CAMERACONTROLLER_H

#include <QObject>
#include <QTimer>

class AviRecorderWorker;
class CameraCapture;
class FrameStore;

// 管理摄像头、两种工作模式、U 盘状态和录像线程的 QML 控制器

class CameraController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(int mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(bool cameraOpen READ cameraOpen NOTIFY cameraOpenChanged)
    Q_PROPERTY(bool cameraBusy READ cameraBusy NOTIFY cameraBusyChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(bool stopping READ stopping NOTIFY stoppingChanged)
    Q_PROPERTY(int recordingSeconds READ recordingSeconds NOTIFY recordingSecondsChanged)
    Q_PROPERTY(bool usbReady READ usbReady NOTIFY usbReadyChanged)
    Q_PROPERTY(QString usbStatus READ usbStatus NOTIFY usbStatusChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString lastSavedPath READ lastSavedPath NOTIFY lastSavedPathChanged)
    Q_PROPERTY(quint64 previewRevision READ previewRevision NOTIFY previewRevisionChanged)

public:
    /**
     * @brief 定义 QML 可选择的工作模式。
     */
    enum Mode
    {
        Mode1 = 1,
        Mode2 = 2
    };
    Q_ENUM(Mode)

    /**
     * @brief 构造控制器并立即检查 U 盘状态。
     * @param frameStore 最新帧存储区，不转移所有权。
     * @param parent Qt 父对象。
     */
    explicit CameraController(FrameStore *frameStore, QObject *parent = nullptr);

    /**
     * @brief 析构控制器并释放采集、录像线程。
     */
    ~CameraController() override;

    /** @brief 获取当前状态名称。 */
    QString state() const;
    /** @brief 获取当前模式。 */
    int mode() const;
    /** @brief 返回摄像头是否已成功产生画面。 */
    bool cameraOpen() const;
    /** @brief 返回摄像头是否正在启动或关闭。 */
    bool cameraBusy() const;
    /** @brief 返回是否正在写入录像。 */
    bool recording() const;
    /** @brief 返回 AVI 是否处于停止收尾阶段。 */
    bool stopping() const;
    /** @brief 获取本段录像经过的秒数。 */
    int recordingSeconds() const;
    /** @brief 返回 /mnt/usb 是否已挂载且可写。 */
    bool usbReady() const;
    /** @brief 获取 U 盘状态说明。 */
    QString usbStatus() const;
    /** @brief 获取当前操作状态说明。 */
    QString statusMessage() const;
    /** @brief 获取最近错误。 */
    QString errorMessage() const;
    /** @brief 获取最近成功保存的录像路径。 */
    QString lastSavedPath() const;
    /** @brief 获取预览图像版本号，用于刷新 QML 图像缓存。 */
    quint64 previewRevision() const;

    /**
     * @brief 开启固定的 /dev/video1 摄像头。
     */
    Q_INVOKABLE void openCamera();

    /**
     * @brief 在非录像状态停止摄像头并释放资源。
     */
    Q_INVOKABLE void closeCamera();

    /**
     * @brief 切换 Mode1 或 Mode2，摄像头已开启时保持不断流。
     * @param requestedMode CameraController::Mode1 或 Mode2。
     */
    Q_INVOKABLE void setMode(int requestedMode);

    /**
     * @brief 在 Mode2 创建录像线程并开始写入 U 盘。
     */
    Q_INVOKABLE void startRecording();

    /**
     * @brief 请求停止录像并开始 AVI 安全收尾。
     */
    Q_INVOKABLE void stopRecording();

    /**
     * @brief 重新检查 /mnt/usb 是否已挂载且可写。
     */
    Q_INVOKABLE void refreshUsbStatus();

    /**
     * @brief 处理 QML 关闭请求；录像中会拒绝关闭。
     * @return 可以安全退出时返回 true。
     */
    Q_INVOKABLE bool requestExit();

Q_SIGNALS:
    void stateChanged();
    void modeChanged();
    void cameraOpenChanged();
    void cameraBusyChanged();
    void recordingChanged();
    void stoppingChanged();
    void recordingSecondsChanged();
    void usbReadyChanged();
    void usbStatusChanged();
    void statusMessageChanged();
    void errorMessageChanged();
    void lastSavedPathChanged();
    void previewRevisionChanged();

private Q_SLOTS:
    /** @brief 处理摄像头第一帧到达。 */
    void onCaptureStarted(int width, int height, int bytesPerLine);
    /** @brief 处理摄像头线程错误。 */
    void onCaptureError(const QString &message);
    /** @brief 处理摄像头线程退出。 */
    void onCaptureStopped();
    /** @brief 处理录像文件成功建立。 */
    void onRecordingStarted();
    /** @brief 处理录像线程收尾结果。 */
    void onRecordingFinished(bool success, const QString &path,
                             const QString &message);
    /** @brief 每秒更新录像计时。 */
    void onRecordingTimer();
    /** @brief 以固定频率读取最新帧版本并刷新 QML 预览。 */
    void onPreviewTimer();

private:
    /** @brief 设置状态名称和用户提示。 */
    void setState(const QString &state, const QString &message);
    /** @brief 设置并通知错误信息。 */
    void setErrorMessage(const QString &message);
    /** @brief 检查路径是否为真实挂载点。 */
    bool isMountedPath(const QString &path) const;
    /** @brief 为下一段录像生成唯一的临时路径与最终路径。 */
    bool buildRecordingPaths(QString *partPath, QString *finalPath);
    /** @brief 删除并清空已结束的录像线程对象。 */
    void releaseRecorder();

    FrameStore *m_frameStore = nullptr;
    CameraCapture *m_capture = nullptr;
    AviRecorderWorker *m_recorder = nullptr;
    QTimer m_recordingTimer;
    QTimer m_previewTimer;
    QString m_state = QStringLiteral("CameraClosed");
    int m_mode = Mode1;
    bool m_cameraOpen = false;
    bool m_cameraBusy = false;
    bool m_recording = false;
    bool m_stopping = false;
    int m_recordingSeconds = 0;
    bool m_usbReady = false;
    QString m_usbStatus;
    QString m_statusMessage = QStringLiteral("摄像头未开启");
    QString m_errorMessage;
    QString m_lastSavedPath;
    quint64 m_previewRevision = 0;
};

#endif // CAMERACONTROLLER_H
