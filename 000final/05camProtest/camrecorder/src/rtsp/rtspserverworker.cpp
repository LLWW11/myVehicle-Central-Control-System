#include "rtspserverworker.h"

#include "jpegframestore.h"

namespace
{
    // JPEG 实际只有约 15FPS(JpegEncoderWorker 每 30FPS 抽 1),
    // 这里轮询频率取高一点只为降低画面延迟,大多数调用是无事发生。
    constexpr int kPushIntervalMs = 20;
    // caps 与 PTS 都按 15FPS 声明,与真实产出频率一致。
    constexpr int kStreamFps = 15;
}

/* =========================== 生命周期 =========================== */

RtspServerWorker::RtspServerWorker(JpegFrameStore *store,
                                   int port,
                                   const QString &mountPath,
                                   QObject *parent)
    : QThread(parent),
      m_store(store),
      m_port(port),
      m_mountPath(mountPath)
{
}

RtspServerWorker::~RtspServerWorker()
{
    requestStop();
    wait(5000); // 与其它 Worker 保持一致的兜底超时
}

/**
 * 请求停止。三件事:
 *  1) 原子置位,让 pushLatestFrame 的定时源自行拆除;
 *  2) 把 "g_main_loop_quit()" 作为 idle 回调投递进目标 context —— 这是 GLib
 *     明确允许跨线程使用的 API(g_main_context_invoke 系列线程安全),
 *     保证 quit 由 loop 所属线程亲手执行;
 *  3) 全程持 m_loopMutex,避免与 run() 中还在创建阶段的手柄竞争。
 */
void RtspServerWorker::requestStop()
{
    m_stopRequested.storeRelease(1);

    QMutexLocker locker(&m_loopMutex);
    if (m_context == nullptr || m_loop == nullptr)
        return; // run 还没建好/已销毁,置位标记足以让后续流程自杀

    g_main_context_invoke_full(m_context,
                               G_PRIORITY_DEFAULT,
                               &RtspServerWorker::onStopIdle,
                               this,
                               nullptr);
}

gboolean RtspServerWorker::onStopIdle(gpointer userData)
{
    auto *self = static_cast<RtspServerWorker *>(userData);
    if (self->m_loop != nullptr)
        g_main_loop_quit(self->m_loop);
    return G_SOURCE_REMOVE; // 一次性回调
}

/* ============================ 主循环 ============================ */

void RtspServerWorker::createPipelineAndListen()
{
    m_context = g_main_context_new();
    m_loop = g_main_loop_new(m_context, FALSE);

    m_server = gst_rtsp_server_new();
    gst_rtsp_server_set_service(m_server,
                                QByteArray::number(m_port).constData());

    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(m_server);

    m_factory = gst_rtsp_media_factory_new();
    gst_rtsp_media_factory_set_launch(
        m_factory,
        "( "
        "appsrc name=jpegsrc "
        "is-live=true "
        "format=time "
        "block=false "
        "caps=\"image/jpeg,width=(int)640,height=(int)480,"
        "framerate=(fraction)"
        "15/1\" "
        "! queue max-size-buffers=2 leaky=downstream "
        "! jpegparse "
        "! rtpjpegpay name=pay0 pt=26 "
        ")");
    gst_rtsp_media_factory_set_shared(m_factory, TRUE);

    g_signal_connect(m_factory, "media-configure",
                     G_CALLBACK(&RtspServerWorker::onMediaConfigure), this);
    gst_rtsp_mount_points_add_factory(mounts,
                                      m_mountPath.toUtf8().constData(),
                                      m_factory);
    g_object_unref(mounts); // add_factory 已引用我们这份,松手

    GError *error = nullptr;
    m_serverSource = gst_rtsp_server_create_source(m_server, nullptr, &error);
    if (m_serverSource == nullptr)
    {
        QString message = QStringLiteral("创建 RTSP 监听失败(端口 %1 被占用?)")
                              .arg(m_port);
        if (error != nullptr)
        {
            message += QStringLiteral(":") +
                       QString::fromUtf8(error->message);
            g_error_free(error);
        }
        qWarning("%s", qPrintable(message));
        Q_EMIT serverError(message);
        return;
    }

    // 用私有 context 时官方推荐 create_source + 手动 attach,
    // 生命周期在我们手里,方便随时整体拆除
    g_source_attach(m_serverSource, m_context);

    // 20ms 一次"看看有没有新帧"
    m_pushSource = g_timeout_source_new(kPushIntervalMs);
    g_source_set_callback(m_pushSource,
                          &RtspServerWorker::onPushTimer,
                          this,
                          nullptr);
    g_source_attach(m_pushSource, m_context);

    Q_EMIT serverStarted();
    // 注意:这里不进入主循环 —— g_main_loop_run 必须在 run() 中锁外执行,
    // 否则 requestStop() 会被 m_loopMutex 阻塞而无法投递 quit(死锁)。
}

void RtspServerWorker::run()
{
    gst_init(nullptr, nullptr); // 幂等,重复调用无副作用

    {
        QMutexLocker locker(&m_loopMutex);
        createPipelineAndListen(); // 装配资源;失败时内部 Q_EMIT serverError 后返回
    }

    // 主循环必须锁外运行:仅当监听建立成功且未被要求停止时才进入;
    // 若刚 start 就被要求停止,则跳过循环直接收摊
    if (m_stopRequested.loadAcquire() == 0 &&
            m_loop != nullptr && m_serverSource != nullptr)
    {
        g_main_loop_run(m_loop); // 阻塞至 requestStop 投递的 quit 生效
    }

    {
        QMutexLocker locker(&m_loopMutex);
        cleanup();
    }
}

void RtspServerWorker::cleanup()
{
    if (m_pushSource != nullptr)
    {
        g_source_destroy(m_pushSource);
        g_source_unref(m_pushSource);
        m_pushSource = nullptr;
    }
    if (m_serverSource != nullptr)
    {
        g_source_destroy(m_serverSource);
        g_source_unref(m_serverSource);
        m_serverSource = nullptr;
    }
    if (m_appsrc != nullptr)
    {
        gst_object_unref(m_appsrc);
        m_appsrc = nullptr;
    }
    if (m_factory != nullptr)
    {
        gst_object_unref(m_factory);
        m_factory = nullptr;
    }
    if (m_server != nullptr)
    {
        gst_object_unref(m_server);
        m_server = nullptr;
    }
    if (m_loop != nullptr)
    {
        g_main_loop_unref(m_loop);
        m_loop = nullptr;
    }
    if (m_context != nullptr)
    {
        g_main_context_unref(m_context);
        m_context = nullptr;
    }
}

/* ==================== 客户端接入/离开(GStreamer 回调) ==================== */

/**
 * 第一个客户端 PLAY 时被调用:pipeline 已建成,翻出 appsrc 存起来。
 * 之后我们只跟这个 appsrc 打交道,GStreamer 自己驱动 parse/pay/发送。
 */
void RtspServerWorker::onMediaConfigure(GstRTSPMediaFactory *,
                                        GstRTSPMedia *media,
                                        gpointer userData)
{
    auto *self = static_cast<RtspServerWorker *>(userData);

    GstElement *element = gst_rtsp_media_get_element(media);
    GstElement *appsrc = gst_bin_get_by_name_recurse_up(
        GST_BIN(element), "jpegsrc");
    gst_object_unref(element);

    if (appsrc == nullptr)
    {
        qWarning("[rtsp] 未能在 media 中找到 appsrc(jpegsrc)");
        return;
    }

    if (self->m_appsrc != nullptr)
        gst_object_unref(self->m_appsrc);
    self->m_appsrc = appsrc; // get_by_name 转移所有权给我们

    self->m_previousGeneration = 0; // 新一轮会话从头开始比对代数
    self->m_frameIndex = 0;

    g_signal_connect(media, "unprepared",
                     G_CALLBACK(&RtspServerWorker::onMediaUnprepared), self);
}

/** 最后一个客户端离开,media 拆除:归还引用、回到待机状态 */
void RtspServerWorker::onMediaUnprepared(GstRTSPMedia *, gpointer userData)
{
    auto *self = static_cast<RtspServerWorker *>(userData);

    if (self->m_appsrc != nullptr)
    {
        gst_object_unref(self->m_appsrc);
        self->m_appsrc = nullptr;
    }
    self->m_previousGeneration = 0;
    self->m_frameIndex = 0;
}

/* ========================= 推帧(核心中的核心) ========================= */

gboolean RtspServerWorker::onPushTimer(gpointer userData)
{
    auto *self = static_cast<RtspServerWorker *>(userData);
    return self->pushLatestFrame();
}

gboolean RtspServerWorker::pushLatestFrame()
{
    if (m_stopRequested.loadAcquire() != 0)
        return G_SOURCE_REMOVE; // 连同定时源一起退出

    if (m_appsrc == nullptr || m_store == nullptr)
        return G_SOURCE_CONTINUE; // 没有观众,白白路过

    const JpegFrame frame = m_store->latest(); // 内部自旋锁拷贝一份快照

    if (!frame.isValid())
        return G_SOURCE_CONTINUE;
    if (frame.generation == m_previousGeneration)
        return G_SOURCE_CONTINUE; // 还是老画面,不发重复帧

    m_previousGeneration = frame.generation;

    const gsize size = static_cast<gsize>(frame.bytes.size());
    GstBuffer *buffer = gst_buffer_new_allocate(nullptr, size, nullptr);
    if (buffer == nullptr)
        return G_SOURCE_CONTINUE;

    gst_buffer_fill(buffer, 0, frame.bytes.constData(), size);

    // RTP 时间戳体系:声明 15FPS → 单帧时长 ≈66.67ms,按序号递增
    const GstClockTime duration =
        gst_util_uint64_scale_int(1, GST_SECOND, kStreamFps);
    GST_BUFFER_PTS(buffer) = m_frameIndex * duration;
    GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DURATION(buffer) = duration;
    ++m_frameIndex;

    gst_app_src_push_buffer(GST_APP_SRC(m_appsrc), buffer);
    // ⚠️ 所有权已交给 appsrc,绝不能再 unref 这块 buffer!

    return G_SOURCE_CONTINUE;
}
