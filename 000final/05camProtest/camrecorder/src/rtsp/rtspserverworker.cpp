#include "rtspserverworker.h"

#include "jpegframestore.h"

namespace
{
    constexpr int kPushIntervalMs = 20; // 20ms推一帧,但是做了是否最新帧判断，实际推流是15 Fps ~ 67ms,
    constexpr int kStreamFps = 15;      // caps 与 PTS 都按 15FPS 声明,与真实产出频率一致
}

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
    wait(5000); // 兜底超时
}

void RtspServerWorker::requestStop()
{
    m_stopRequested.storeRelease(1);

    QMutexLocker locker(&m_loopMutex); // 离开作用域自动解锁
    if (m_context == nullptr || m_loop == nullptr)
        return;

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

void RtspServerWorker::createPipelineAndListen()
{
    m_context = g_main_context_new();
    m_loop = g_main_loop_new(m_context, FALSE);

    m_server = gst_rtsp_server_new();
    gst_rtsp_server_set_service(m_server, QByteArray::number(m_port).constData());

    // RTSP挂载点
    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(m_server);

    m_factory = gst_rtsp_media_factory_new();
    // 一整条管线
    gst_rtsp_media_factory_set_launch(
        m_factory,
        "( "
        "appsrc name=jpegsrc " // 喂数据入口
        "is-live=true "        // 实时产生
        "format=time "
        "block=false " // buffer不阻塞
        "caps=\"image/jpeg,width=(int)640,height=(int)480,"
        "framerate=(fraction)"
        "15/1 \" " // 指定视频流格式
        "! queue max-size-buffers=2 leaky=downstream "
        "! jpegparse "                  // 解析补齐RTP
        "! rtpjpegpay name=pay0 pt=26 " // 只有一条视频流
        ")");

    gst_rtsp_media_factory_set_shared(m_factory, TRUE); // 一条pipe共享多个RTP会话

    g_signal_connect(m_factory, "media-configure",
                     G_CALLBACK(&RtspServerWorker::onMediaConfigure), //
                     this);
    gst_rtsp_mount_points_add_factory(mounts,
                                      m_mountPath.toUtf8().constData(),
                                      m_factory);
    g_object_unref(mounts);

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
    // 准备好之后可以开始
}

void RtspServerWorker::run()
{
    gst_init(nullptr, nullptr);

    {
        QMutexLocker locker(&m_loopMutex);
        createPipelineAndListen();
    }

    // 仅当监听建立成功且未被要求停止时才进入;
    // 若刚 start 就被要求停止,则跳过循环直接收摊
    if (m_stopRequested.loadAcquire() == 0 &&
        m_loop != nullptr &&
        m_serverSource != nullptr)
    {
        g_main_loop_run(m_loop); // 阻塞
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

//  第一个客户端 PLAY 时被调用:pipeline 已建成,翻出 appsrc 存起来
//  只跟 appsrc 打交道,GStreamer 自己驱动 parse/pay/发送
void RtspServerWorker::onMediaConfigure(GstRTSPMediaFactory *Gstfactory,
                                        GstRTSPMedia *media, // 媒体会话对象
                                        gpointer userData)
{
    auto *self = static_cast<RtspServerWorker *>(userData); // 强转 后续能访问成员变量

    GstElement *element = gst_rtsp_media_get_element(media);
    GstElement *appsrc = gst_bin_get_by_name_recurse_up(GST_BIN(element), "jpegsrc");
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

// 最后一个客户端离开,media 拆除:归还引用、回到待机状态
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

// 推帧
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
        return G_SOURCE_CONTINUE;

    const JpegFrame frame = m_store->latest(); // 内部自旋锁拷贝一份最新帧

    if (!frame.isValid())
        return G_SOURCE_CONTINUE;
    if (frame.generation == m_previousGeneration) // 不重复推帧
        return G_SOURCE_CONTINUE;

    m_previousGeneration = frame.generation;

    const gsize size = static_cast<gsize>(frame.bytes.size());
    GstBuffer *buffer = gst_buffer_new_allocate(nullptr, size, nullptr);
    if (buffer == nullptr)
        return G_SOURCE_CONTINUE;

    gst_buffer_fill(buffer, 0, frame.bytes.constData(), size);

    // RTP 时间戳体系:声明 15FPS → 单帧时长 ≈66.67ms,按序号递增
    const GstClockTime duration = gst_util_uint64_scale_int(1, GST_SECOND, kStreamFps);
    GST_BUFFER_PTS(buffer) = m_frameIndex * duration;
    GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DURATION(buffer) = duration;
    m_frameIndex++;

    gst_app_src_push_buffer(GST_APP_SRC(m_appsrc), buffer);
    // 所有权已交给 appsrc,绝不能再 unref 这块 buffer!

    return G_SOURCE_CONTINUE;
}
