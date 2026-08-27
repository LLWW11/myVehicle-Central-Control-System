#ifndef RTSPSERVERWORKER_H
#define RTSPSERVERWORKER_H

#include <QAtomicInt>
#include <QMutex>
#include <QString>
#include <QThread>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/rtsp-server/rtsp-server.h>

class JpegFrameStore;

/**
 * @brief 从 JpegFrameStore 拉 JPEG 帧,经 GStreamer 以 RTP/JPEG 向外提供 RTSP 直播
 *
 * 生命周期与 AviRecorderWorker 相同:由 CameraController 在 GUI 线程创建,
 * requestStop() 后由其 wait + delete。内部所有 GstRTSPServer/GLib 回调都在
 * 本线程的 private GMainLoop 里串行执行,因此 m_appsrc 等 GLib 手柄不需要加锁,
 * 唯独它们在 run() 里创建、又会被 GUI 线程的 requestStop() 访问,由 m_loopMutex 保护。
 */
class RtspServerWorker : public QThread
{
    Q_OBJECT

public:
    explicit RtspServerWorker(JpegFrameStore *store,
                              int port = 8554,
                              const QString &mountPath = QStringLiteral("/camera"),
                              QObject *parent = nullptr);
    ~RtspServerWorker() override;

    void requestStop(); // 线程安全,可在任意线程调用

signals:
    void serverStarted();                     // 端口已监听成功
    void serverError(const QString &message); // 监听失败,线程即将结束

protected:
    void run() override;

private:
    /* ---- GLib loop 线程回调(客户端接入/断开/定时推帧/停止投递) ---- */
    static void onMediaConfigure(GstRTSPMediaFactory *factory,
                                 GstRTSPMedia *media,
                                 gpointer userData);
    static void onMediaUnprepared(GstRTSPMedia *media,
                                  gpointer userData);
    static gboolean onStopIdle(gpointer userData);
    static gboolean onPushTimer(gpointer userData);
    gboolean pushLatestFrame();

    void createPipelineAndListen(); // run() 内使用
    void cleanup();                 // run() 退出前统一回收

    JpegFrameStore *m_store = nullptr;
    int m_port = 8554;
    QString m_mountPath;

    QAtomicInt m_stopRequested{0};
    QMutex m_loopMutex; // 保护下面四个 GLib 手柄的生命周期

    GMainContext *m_context = nullptr; // 私有事件上下文(不属于 default)
    GMainLoop *m_loop = nullptr;
    GstRTSPServer *m_server = nullptr;
    GstRTSPMediaFactory *m_factory = nullptr;
    GSource *m_serverSource = nullptr; // server 的监听 source
    GSource *m_pushSource = nullptr;   // 20ms 推帧定时器 source

    /* ---- 下面三个只在 GLib loop 线程里读写,无需互斥量 ---- */
    GstElement *m_appsrc = nullptr; // 客户端连接期间才有值
    quint64 m_previousGeneration = 0;
    quint64 m_frameIndex = 0; // 用于计算 PTS 时间戳
};

#endif // RTSPSERVERWORKER_H
