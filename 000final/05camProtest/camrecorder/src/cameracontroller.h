#ifndef CAMERACONTROLLER_H
#define CAMERACONTROLLER_H

#include <QObject>
#include <QTimer>

class AviRecorderWorker;
class CameraCapture;
class FrameStore;
class JpegEncoderWorker;
class JpegFrameStore;
class RtspServerWorker;
/**
 * @brief 管理摄像头、两种工作模式、U 盘状态和录像线程的 QML 控制器
 */
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
    Q_PROPERTY(bool rtspEnabled READ rtspEnabled NOTIFY rtspEnabledChanged)
    Q_PROPERTY(QString rtspStatus READ rtspStatus NOTIFY rtspStatusChanged)
    Q_PROPERTY(quint64 previewRevision READ previewRevision NOTIFY previewRevisionChanged)

public:
    enum Mode
    {
        Mode1 = 1,
        Mode2 = 2
    };
    Q_ENUM(Mode)

    // 构造控制器并立即检查 U 盘状态
    explicit CameraController(FrameStore *frameStore, QObject *parent = nullptr);
    ~CameraController() override;

    QString state() const;         // 获取当前状态名称
    int mode() const;              // 获取当前模式
    bool cameraOpen() const;       // 摄像头是否已成功产生画面
    bool cameraBusy() const;       // 摄像头是否正在启动或者关闭
    bool recording() const;        // 是否正在录像
    bool stopping() const;         // AVI 是否处于停止收尾阶段
    int recordingSeconds() const;  // 获取本段录像经过的秒数
    bool usbReady() const;         // /mnt/usb是否已挂载且可写
    QString usbStatus() const;     // U 盘状态
    QString statusMessage() const; // 当前操作状态说明
    QString errorMessage() const;
    QString lastSavedPath() const; // 成功保存的录像路径
    quint64 previewRevision() const;
    bool rtspEnabled() const;
    QString rtspStatus() const;

    Q_INVOKABLE void openCamera();
    Q_INVOKABLE void closeCamera();
    Q_INVOKABLE void setMode(int requestedMode); // 切换 Mode1 或 Mode2，摄像头已开启时保持不断流
    Q_INVOKABLE void startRecording();           // 在 Mode2 创建录像线程并开始写入 U 盘
    Q_INVOKABLE void stopRecording();            // 请求停止录像并开始 AVI 安全收尾
    Q_INVOKABLE void startRtsp();                // 开启 RTSP 推流（JpegFrameStore → RTP/JPEG）
    Q_INVOKABLE void stopRtsp();                 // 关闭 RTSP 推流
    Q_INVOKABLE void refreshUsbStatus();         // /mnt/usb 是否已挂载且可写
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
    void rtspEnabledChanged();
    void rtspStatusChanged();

private Q_SLOTS:
    void onCaptureStarted(int width,
                          int height,
                          int bytesPerLine);     // 处理摄像头第一帧到达
    void onCaptureError(const QString &message); // 处理摄像头线程错误
    void onCaptureStopped();                     // 处理摄像头线程退出
    void onRecordingStarted();                   // 处理录像文件成功建立
    void onEncoderError(const QString &message); // 处理 JPEG 编码线程错误
    void onRecordingFinished(bool success,
                             const QString &path,
                             const QString &message); // 处理录像线程收尾结果
    void onRecordingTimer();                          // 每秒更新录像计时
    void onPreviewTimer();
    void onRtspServerError(const QString &message); // 处理 RTSP 线程启动失败

private:
    void setState(const QString &state,
                  const QString &message);         // 设置状态名称和用户提示
    void setErrorMessage(const QString &message);  // 设置并通知错误信息
    void startJpegEncoder();                       // 启动共享JPEG编码线程
    void releaseJpegEncoder();                     // 停止并释放共享JPEG编码线程，清空共享 JPEG 帧
    bool isMountedPath(const QString &path) const; // 检查路径是否为真实挂载点
    bool buildRecordingPaths(QString *partPath,
                             QString *finalPath); // 为下一段录像生成唯一的临时路径与最终路径
    void releaseRecorder();                       // 删除并清空已结束的录像线程对象
    void releaseRtsp();                           // 停止并释放 RTSP 推流线程对象
    JpegFrameStore *m_jpegFrameStore = nullptr;
    JpegEncoderWorker *m_jpegEncoderWorker = nullptr;
    FrameStore *m_frameStore = nullptr;
    CameraCapture *m_capture = nullptr;
    AviRecorderWorker *m_recorder = nullptr;
    RtspServerWorker *m_rtspServer = nullptr;
    bool m_rtspEnabled = false;
    QString m_rtspStatus = QStringLiteral("RTSP 未开启");
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
