#include "cameracontroller.h"

#include "camera/cameracapture.h"
#include "camera/framestore.h"
#include "recorder/avirecorderworker.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>

namespace {
const QString kUsbPath = QStringLiteral("/mnt/usb");
}

/**
 * @brief 构造控制器并立即检查 U 盘状态。
 */
CameraController::CameraController(FrameStore *frameStore, QObject *parent)
    : QObject(parent)
    , m_frameStore(frameStore)
{
    m_recordingTimer.setInterval(1000);
    m_recordingTimer.setSingleShot(false);
    connect(&m_recordingTimer, &QTimer::timeout,
            this, &CameraController::onRecordingTimer);
    m_previewTimer.setInterval(33);
    m_previewTimer.setSingleShot(false);
    connect(&m_previewTimer, &QTimer::timeout,
            this, &CameraController::onPreviewTimer);
    refreshUsbStatus();
}

/**
 * @brief 析构控制器并释放采集、录像线程。
 */
CameraController::~CameraController()
{
    if (m_recorder != nullptr && m_recorder->isRunning()) {
        m_recorder->requestStop();
        m_recorder->wait(10000);
    }
    releaseRecorder();

    if (m_capture != nullptr) {
        m_capture->requestStop();
        m_capture->wait(3000);
        delete m_capture;
        m_capture = nullptr;
    }
}

QString CameraController::state() const { return m_state; }
int CameraController::mode() const { return m_mode; }
bool CameraController::cameraOpen() const { return m_cameraOpen; }
bool CameraController::cameraBusy() const { return m_cameraBusy; }
bool CameraController::recording() const { return m_recording; }
bool CameraController::stopping() const { return m_stopping; }
int CameraController::recordingSeconds() const { return m_recordingSeconds; }
bool CameraController::usbReady() const { return m_usbReady; }
QString CameraController::usbStatus() const { return m_usbStatus; }
QString CameraController::statusMessage() const { return m_statusMessage; }
QString CameraController::errorMessage() const { return m_errorMessage; }
QString CameraController::lastSavedPath() const { return m_lastSavedPath; }
quint64 CameraController::previewRevision() const { return m_previewRevision; }

/**
 * @brief 开启固定的 /dev/video1 摄像头。
 */
void CameraController::openCamera()
{
    if (m_cameraOpen || m_cameraBusy || m_recording || m_stopping)
        return;

    if (m_capture != nullptr) {
        if (m_capture->isRunning())
            return;
        delete m_capture;
    }

    setErrorMessage(QString());
    m_cameraBusy = true;
    emit cameraBusyChanged();
    setState(QStringLiteral("Opening"), QStringLiteral("正在打开 /dev/video1…"));

    m_capture = new CameraCapture(m_frameStore, this);
    connect(m_capture, &CameraCapture::captureStarted,
            this, &CameraController::onCaptureStarted);
    connect(m_capture, &CameraCapture::captureError,
            this, &CameraController::onCaptureError,
            Qt::QueuedConnection);
    connect(m_capture, &CameraCapture::captureStopped,
            this, &CameraController::onCaptureStopped,
            Qt::QueuedConnection);
    m_capture->start();
}

/**
 * @brief 在非录像状态停止摄像头并释放资源。
 */
void CameraController::closeCamera()
{
    if (m_recording || m_stopping) {
        setErrorMessage(QStringLiteral("请先停止录像，再关闭摄像头"));
        return;
    }
    if (m_capture == nullptr || !m_capture->isRunning()) {
        m_cameraOpen = false;
        m_cameraBusy = false;
        setState(QStringLiteral("CameraClosed"), QStringLiteral("摄像头未开启"));
        return;
    }

    m_cameraBusy = true;
    emit cameraBusyChanged();
    setState(QStringLiteral("Closing"), QStringLiteral("正在关闭摄像头…"));
    m_capture->requestStop();
}

/**
 * @brief 切换 Mode1 或 Mode2，摄像头已开启时保持不断流。
 */
void CameraController::setMode(int requestedMode)
{
    if (requestedMode != Mode1 && requestedMode != Mode2)
        return;
    if (m_recording || m_stopping) {
        setErrorMessage(QStringLiteral("录像期间不能切换模式，请先停止录像"));
        return;
    }
    if (m_mode == requestedMode)
        return;

    m_mode = requestedMode;
    emit modeChanged();
    setErrorMessage(QString());
    if (m_cameraOpen) {
        setState(m_mode == Mode1 ? QStringLiteral("Mode1Preview")
                                 : QStringLiteral("Mode2Idle"),
                 m_mode == Mode1 ? QStringLiteral("Mode1：仅采集和预览")
                                 : QStringLiteral("Mode2：可开始录像"));
    } else {
        setState(QStringLiteral("CameraClosed"),
                 m_mode == Mode1 ? QStringLiteral("Mode1 已选择，请开启摄像头")
                                 : QStringLiteral("Mode2 已选择，请开启摄像头"));
    }
}

/**
 * @brief 在 Mode2 创建录像线程并开始写入 U 盘。
 */
void CameraController::startRecording()
{
    if (m_mode != Mode2) {
        setErrorMessage(QStringLiteral("只有 Mode2 可以录像"));
        return;
    }
    if (!m_cameraOpen || m_cameraBusy) {
        setErrorMessage(QStringLiteral("请先开启摄像头并等待预览画面"));
        return;
    }
    if (m_recording || m_stopping || m_recorder != nullptr)
        return;

    refreshUsbStatus();
    if (!m_usbReady) {
        setErrorMessage(QStringLiteral("/mnt/usb 未挂载或不可写，无法开始录像"));
        return;
    }

    QString partPath;
    QString finalPath;
    if (!buildRecordingPaths(&partPath, &finalPath))
        return;

    setErrorMessage(QString());
    m_recordingSeconds = 0;
    emit recordingSecondsChanged();
    m_stopping = true; // 文件成功建立前视为忙状态，阻止其他操作。
    emit stoppingChanged();
    setState(QStringLiteral("StartingRecord"), QStringLiteral("正在创建录像文件…"));

    m_recorder = new AviRecorderWorker(m_frameStore, partPath, finalPath, this);
    connect(m_recorder, &AviRecorderWorker::recordingStarted,
            this, &CameraController::onRecordingStarted,
            Qt::QueuedConnection);
    connect(m_recorder, &AviRecorderWorker::recordingFinished,
            this, &CameraController::onRecordingFinished,
            Qt::QueuedConnection);
    m_recorder->start();
}

/**
 * @brief 请求停止录像并开始 AVI 安全收尾。
 */
void CameraController::stopRecording()
{
    if (!m_recording || m_recorder == nullptr || m_stopping)
        return;

    m_stopping = true;
    emit stoppingChanged();
    setState(QStringLiteral("Stopping"), QStringLiteral("正在完成 AVI 收尾，请稍候…"));
    m_recorder->requestStop();
}

/**
 * @brief 重新检查 /mnt/usb 是否已挂载且可写。
 */
void CameraController::refreshUsbStatus()
{
    const QFileInfo info(kUsbPath);
    const bool ready = info.exists() && info.isDir() && info.isWritable()
            && isMountedPath(kUsbPath);
    const QString status = ready
            ? QStringLiteral("U盘已挂载且可写：/mnt/usb")
            : QStringLiteral("U盘未就绪：请将U盘挂载到 /mnt/usb");

    if (m_usbReady != ready) {
        m_usbReady = ready;
        emit usbReadyChanged();
    }
    if (m_usbStatus != status) {
        m_usbStatus = status;
        emit usbStatusChanged();
    }
}

/**
 * @brief 处理 QML 关闭请求；录像中会拒绝关闭。
 */
bool CameraController::requestExit()
{
    if (m_recording || m_stopping || m_recorder != nullptr) {
        setErrorMessage(QStringLiteral("录像正在进行或收尾，请先停止录像后再退出"));
        return false;
    }
    if (m_capture != nullptr && m_capture->isRunning()) {
        m_capture->requestStop();
        if (!m_capture->wait(3000)) {
            setErrorMessage(QStringLiteral("摄像头线程尚未停止，请稍后重试退出"));
            return false;
        }
    }
    return true;
}

/**
 * @brief 处理摄像头第一帧到达。
 */
void CameraController::onCaptureStarted(int width, int height, int bytesPerLine)
{
    Q_UNUSED(bytesPerLine)
    if (width != 640 || height != 480) {
        setErrorMessage(QStringLiteral("摄像头实际分辨率不是 640×480"));
        closeCamera();
        return;
    }

    m_cameraBusy = false;
    emit cameraBusyChanged();
    if (!m_cameraOpen) {
        m_cameraOpen = true;
        emit cameraOpenChanged();
    }
    m_previewTimer.start();
    setState(m_mode == Mode1 ? QStringLiteral("Mode1Preview")
                             : QStringLiteral("Mode2Idle"),
             m_mode == Mode1 ? QStringLiteral("Mode1：仅采集和预览")
                             : QStringLiteral("Mode2：可开始录像"));
}

/**
 * @brief 处理摄像头线程错误。
 */
void CameraController::onCaptureError(const QString &message)
{
    setErrorMessage(message);
    if (m_recorder == nullptr || !m_recorder->isRunning())
        return;

    if (m_recording) {
        stopRecording();
    } else {
        // 文件创建阶段也必须唤醒录像线程，避免摄像头掉线后一直等待首帧。
        m_stopping = true;
        emit stoppingChanged();
        setState(QStringLiteral("Stopping"),
                 QStringLiteral("摄像头异常，正在停止录像线程…"));
        m_recorder->requestStop();
    }
}

/**
 * @brief 处理摄像头线程退出。
 */
void CameraController::onCaptureStopped()
{
    m_previewTimer.stop();
    const bool wasOpen = m_cameraOpen;
    m_cameraOpen = false;
    m_cameraBusy = false;
    if (wasOpen)
        emit cameraOpenChanged();
    emit cameraBusyChanged();
    ++m_previewRevision;
    emit previewRevisionChanged();
    setState(QStringLiteral("CameraClosed"), QStringLiteral("摄像头未开启"));
}

/**
 * @brief 处理录像文件成功建立。
 */
void CameraController::onRecordingStarted()
{
    m_stopping = false;
    emit stoppingChanged();
    m_recording = true;
    emit recordingChanged();
    m_recordingTimer.start();
    setState(QStringLiteral("Recording"), QStringLiteral("正在录像到 /mnt/usb"));
}

/**
 * @brief 处理录像线程收尾结果。
 */
void CameraController::onRecordingFinished(bool success, const QString &path,
                                           const QString &message)
{
    m_recordingTimer.stop();
    if (m_recording) {
        m_recording = false;
        emit recordingChanged();
    }
    if (m_stopping) {
        m_stopping = false;
        emit stoppingChanged();
    }

    if (success) {
        m_lastSavedPath = path;
        emit lastSavedPathChanged();
        setErrorMessage(QString());
        setState(m_cameraOpen ? QStringLiteral("Mode2Idle")
                              : QStringLiteral("CameraClosed"),
                 m_cameraOpen ? message
                              : QStringLiteral("录像已保存，但摄像头已经停止"));
    } else {
        setErrorMessage(QStringLiteral("%1；未完成文件保留在：%2")
                        .arg(message, path));
        setState(QStringLiteral("Error"), QStringLiteral("录像失败"));
    }
    releaseRecorder();
    refreshUsbStatus();
}

/**
 * @brief 每秒更新录像计时。
 */
void CameraController::onRecordingTimer()
{
    ++m_recordingSeconds;
    emit recordingSecondsChanged();
}

/**
 * @brief 以固定频率读取最新帧版本并刷新 QML 预览。
 */
void CameraController::onPreviewTimer()
{
    if (m_frameStore == nullptr)
        return;
    const quint64 generation = m_frameStore->latest().generation;
    if (generation == 0 || generation == m_previewRevision)
        return;
    m_previewRevision = generation;
    emit previewRevisionChanged();
}

/**
 * @brief 设置状态名称和用户提示。
 */
void CameraController::setState(const QString &state, const QString &message)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged();
    }
    if (m_statusMessage != message) {
        m_statusMessage = message;
        emit statusMessageChanged();
    }
}

/**
 * @brief 设置并通知错误信息。
 */
void CameraController::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message)
        return;
    m_errorMessage = message;
    emit errorMessageChanged();
}

/**
 * @brief 检查路径是否为真实挂载点。
 */
bool CameraController::isMountedPath(const QString &path) const
{
    const QString canonicalPath = QFileInfo(path).canonicalFilePath();
    if (canonicalPath.isEmpty())
        return false;

    const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &volume : volumes) {
        const QString rootPath = QFileInfo(volume.rootPath()).canonicalFilePath();
        if (volume.isValid() && volume.isReady() && rootPath == canonicalPath)
            return true;
    }
    return false;
}

/**
 * @brief 为下一段录像生成唯一的临时路径与最终路径。
 */
bool CameraController::buildRecordingPaths(QString *partPath, QString *finalPath)
{
    if (partPath == nullptr || finalPath == nullptr)
        return false;

    const QString timestamp = QDateTime::currentDateTime()
            .toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString baseName = QStringLiteral("cam_rec_%1.avi").arg(timestamp);
    *finalPath = QDir(kUsbPath).filePath(baseName);
    *partPath = *finalPath + QStringLiteral(".part");
    if (QFile::exists(*finalPath) || QFile::exists(*partPath)) {
        setErrorMessage(QStringLiteral("同名录像文件已存在，请一秒后重试"));
        return false;
    }
    return true;
}

/**
 * @brief 删除并清空已结束的录像线程对象。
 */
void CameraController::releaseRecorder()
{
    if (m_recorder == nullptr)
        return;
    if (m_recorder->isRunning())
        m_recorder->wait();
    delete m_recorder;
    m_recorder = nullptr;
}
