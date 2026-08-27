你这两个材料可以直接合起来做：GitHub 当前代码负责告诉我们“现在到底怎么实现的”，你上传的方案负责给出“目标架构”。我核对后，整体方向是正确的，但实际写代码时建议稍微调整一下 RTSP 线程实现，让它更适合你现在的 Qt `QThread + FrameStore` 模型。

你当前真正的数据链路是：

```text
/dev/video1
    ↓
CameraCapture
RGB565 640×480 30FPS
    ↓
FrameStore
    ├────────────→ QML Preview
    │
    └────────────→ AviRecorderWorker
                       ↓
                   每2帧取1帧
                       ↓
                  AviWriter
                       ↓
               RGB565 → JPEG
                       ↓
                   MJPEG AVI
```

仓库里的 `CameraCapture` 确实固定使用 `/dev/video1`、RGB565、640×480、30 FPS；`AviRecorderWorker` 目前每两帧取一帧，然后调用 `AviWriter::appendFrame()`，而 JPEG 编码目前就藏在 `AviWriter` 内部。

所以你不能简单写成：

```text
FrameStore
   ├── AviWriter → JPEG
   └── RTSP      → JPEG
```

否则录像+RTSP时，同一帧会 JPEG 编码两次。你上传的方案里最关键的原则就是“JPEG 只编码一次，AVI 和 RTSP 共用”。

最终应该改成：

```text
                         ┌────────────── QML预览
                         │
CameraCapture → FrameStore
                         │
                         ▼
                JpegEncoderWorker
                  30FPS → 15FPS
                  RGB565 → JPEG
                         │
                         ▼
                  JpegFrameStore
                    │          │
                    │          └────→ RtspServerWorker
                    │                    ↓
                    │                  appsrc
                    │                    ↓
                    │                jpegparse
                    │                    ↓
                    │               rtpjpegpay
                    │                    ↓
                    │               GstRTSPServer
                    │
                    └────→ AviRecorderWorker
                                  ↓
                              AviWriter
                                  ↓
                              MJPEG AVI
```

下面按实际修改顺序讲。

------

# 一、第一步先不要碰 RTSP：把 JPEG 编码从 AviWriter 拆出来

这是最重要的一步。

你现在 `aviwriter.cpp` 里的：

```cpp
bool AviWriter::encodeJpeg(const CameraFrame &frame, QByteArray *jpeg)
```

里面包含：

```cpp
JpegErrorManager
JpegEncodeState
onJpegError()

jpeg_create_compress()
jpeg_mem_dest()
RGB565 → RGB888
jpeg_write_scanlines()
jpeg_finish_compress()
```

这些代码本质上跟 AVI 完全没关系。

所以新增：

```text
src/codec/
├── jpegencoder.h
└── jpegencoder.cpp
```

## `jpegencoder.h`

```cpp
#ifndef JPEGENCODER_H
#define JPEGENCODER_H

#include <QByteArray>
#include <QString>

struct CameraFrame;

class JpegEncoder
{
public:
    static bool encode(const CameraFrame &frame,
                       int quality,
                       QByteArray *jpeg,
                       QString *errorString = nullptr);
};

#endif
```

然后把你当前 `AviWriter::encodeJpeg()` 几乎原封不动搬进：

```cpp
JpegEncoder::encode()
```

重点只改几个地方。

原来的：

```cpp
state->compressor.image_width =
        static_cast<JDIMENSION>(m_width);

state->compressor.image_height =
        static_cast<JDIMENSION>(m_height);

jpeg_set_quality(&state->compressor,
                 m_jpegQuality,
                 TRUE);
```

改成：

```cpp
state->compressor.image_width =
        static_cast<JDIMENSION>(frame.width);

state->compressor.image_height =
        static_cast<JDIMENSION>(frame.height);

jpeg_set_quality(&state->compressor,
                 quality,
                 TRUE);
```

原来：

```cpp
std::malloc(static_cast<size_t>(m_width) * 3U);
```

改：

```cpp
std::malloc(static_cast<size_t>(frame.width) * 3U);
```

原来：

```cpp
for (int x = 0; x < m_width; ++x)
```

改：

```cpp
for (int x = 0; x < frame.width; ++x)
```

错误信息原来写：

```cpp
m_errorString = ...
```

现在改：

```cpp
if (errorString != nullptr)
    *errorString = ...;
```

也就是说：

```text
AviWriter::encodeJpeg()
```

不是重新发明一个编码器，而是直接搬家到：

```text
JpegEncoder::encode()
```

这一阶段做完以后，暂时还不要改变录像工作流程。

------

# 二、先修改 AviWriter，让它既支持 RGB565，也支持“现成 JPEG”

现在最关键的是把：

```cpp
AviWriter::appendFrame()
```

拆成两层。

当前代码实际上是：

```cpp
bool AviWriter::appendFrame(const CameraFrame &frame)
{
    QByteArray jpeg;

    if (!encodeJpeg(frame, &jpeg))
        return false;

    // 下面开始写 00dc chunk
    ...
}
```

修改 `aviwriter.h`：

```cpp
bool appendFrame(const CameraFrame &frame);

bool appendJpegFrame(const QByteArray &jpeg);
```

然后把真正写 AVI 的代码移动到：

```cpp
bool AviWriter::appendJpegFrame(const QByteArray &jpeg)
{
    if (m_file == nullptr) {
        m_errorString = QStringLiteral("AVI 文件尚未打开");
        return false;
    }

    if (jpeg.size() < 4
            || static_cast<unsigned char>(jpeg[0]) != 0xff
            || static_cast<unsigned char>(jpeg[1]) != 0xd8) {
        m_errorString = QStringLiteral("JPEG 帧无效");
        return false;
    }

    IndexEntry entry;
    entry.offset = m_chunkOffset;
    entry.size = static_cast<quint32>(jpeg.size());

    m_index.append(entry);

    uchar chunkHeader[8];

    putFourcc(chunkHeader, "00dc");
    putLe32(chunkHeader + 4, entry.size);

    if (!writeBytes(chunkHeader, sizeof(chunkHeader))
            || !writeBytes(jpeg.constData(),
                           static_cast<size_t>(jpeg.size()))) {
        m_index.removeLast();
        return false;
    }

    m_chunkOffset += 8U + entry.size;

    if ((entry.size & 1U) != 0U) {
        const uchar padding = 0;

        if (!writeBytes(&padding, 1)) {
            m_index.removeLast();
            return false;
        }

        ++m_chunkOffset;
    }

    ++m_frames;

    if (entry.size > m_maxFrameSize)
        m_maxFrameSize = entry.size;

    return true;
}
```

然后旧接口暂时保留：

```cpp
bool AviWriter::appendFrame(const CameraFrame &frame)
{
    QByteArray jpeg;
    QString error;

    if (!JpegEncoder::encode(frame,
                             m_jpegQuality,
                             &jpeg,
                             &error)) {
        m_errorString = error;
        return false;
    }

    return appendJpegFrame(jpeg);
}
```

这样代码关系变成：

```text
appendFrame(RGB565)
      ↓
JpegEncoder
      ↓
appendJpegFrame(JPEG)
      ↓
写 AVI
```

这一步完成后，你原来的 `AviRecorderWorker` 一行都可以不改。

先重新编译运行，确认：

```text
开始录像
→ 录像30秒
→ 停止录像
→ AVI正常打开
→ 640×480
→ MJPEG
→ 约15FPS
```

只有这一步正常了，才进入下一步。

------

# 三、新增 JpegFrameStore

你现在已经有一个非常适合这个设计的 `FrameStore`。

它使用：

```cpp
QMutex
QWaitCondition
generation
latest()
waitForNewFrame()
wakeAll()
```

而且只保存最新一帧，消费者慢了直接跳旧帧，非常适合实时视频。

所以压缩后的 JPEG 完全照抄这个设计。

新增：

```text
src/codec/jpegframestore.h
src/codec/jpegframestore.cpp
```

## `jpegframestore.h`

```cpp
#ifndef JPEGFRAMESTORE_H
#define JPEGFRAMESTORE_H

#include <QByteArray>
#include <QMutex>
#include <QWaitCondition>

struct JpegFrame
{
    QByteArray bytes;

    int width = 0;
    int height = 0;

    quint64 generation = 0;

    bool isValid() const
    {
        if (width <= 0 || height <= 0)
            return false;

        if (bytes.size() < 4)
            return false;

        const unsigned char first0 =
                static_cast<unsigned char>(bytes[0]);

        const unsigned char first1 =
                static_cast<unsigned char>(bytes[1]);

        return first0 == 0xff && first1 == 0xd8;
    }
};

class JpegFrameStore
{
public:
    quint64 publish(const QByteArray &jpeg,
                    int width,
                    int height);

    JpegFrame latest() const;

    bool waitForNewFrame(quint64 previousGeneration,
                         int timeoutMs,
                         JpegFrame *frame);

    void clear();

    void wakeAll();

private:
    mutable QMutex m_mutex;
    QWaitCondition m_frameArrived;

    JpegFrame m_latestFrame;

    quint64 m_generation = 0;
};

#endif
```

## `jpegframestore.cpp`

直接仿照你现有 `FrameStore`：

```cpp
#include "jpegframestore.h"

#include <QMutexLocker>

quint64 JpegFrameStore::publish(const QByteArray &jpeg,
                                int width,
                                int height)
{
    QMutexLocker locker(&m_mutex);

    ++m_generation;

    m_latestFrame.bytes = jpeg;
    m_latestFrame.width = width;
    m_latestFrame.height = height;
    m_latestFrame.generation = m_generation;

    m_frameArrived.wakeAll();

    return m_generation;
}

JpegFrame JpegFrameStore::latest() const
{
    QMutexLocker locker(&m_mutex);
    return m_latestFrame;
}

bool JpegFrameStore::waitForNewFrame(
        quint64 previousGeneration,
        int timeoutMs,
        JpegFrame *frame)
{
    if (frame == nullptr)
        return false;

    QMutexLocker locker(&m_mutex);

    if (m_latestFrame.generation <= previousGeneration) {
        m_frameArrived.wait(
                    &m_mutex,
                    static_cast<unsigned long>(timeoutMs));
    }

    if (m_latestFrame.generation <= previousGeneration)
        return false;

    *frame = m_latestFrame;

    return frame->isValid();
}

void JpegFrameStore::clear()
{
    QMutexLocker locker(&m_mutex);

    ++m_generation;

    m_latestFrame = JpegFrame();
    m_latestFrame.generation = m_generation;

    m_frameArrived.wakeAll();
}

void JpegFrameStore::wakeAll()
{
    QMutexLocker locker(&m_mutex);
    m_frameArrived.wakeAll();
}
```

注意这里没有“JPEG 队列”。

仍然只有：

```text
最新一帧 JPEG
```

这是故意的。

RTSP 最忌讳：

```text
网络慢
↓
JPEG越来越多
↓
排队3秒
↓
用户看到3秒以前的画面
```

实时监控应该：

```text
来不及处理
↓
丢旧帧
↓
继续最新帧
```

------

# 四、增加真正的共享 JPEG 编码线程

新增：

```text
jpegencoderworker.h
jpegencoderworker.cpp
```

这个类才是整个重构的核心。

输入：

```text
FrameStore RGB565 30FPS
```

输出：

```text
JpegFrameStore JPEG 15FPS
```

## `jpegencoderworker.h`

```cpp
#ifndef JPEGENCODERWORKER_H
#define JPEGENCODERWORKER_H

#include <QAtomicInt>
#include <QThread>

class FrameStore;
class JpegFrameStore;

class JpegEncoderWorker : public QThread
{
    Q_OBJECT

public:
    JpegEncoderWorker(FrameStore *rawStore,
                      JpegFrameStore *jpegStore,
                      QObject *parent = nullptr);

    ~JpegEncoderWorker() override;

    void requestStop();

signals:
    void encoderError(const QString &message);

protected:
    void run() override;

private:
    FrameStore *m_rawStore = nullptr;
    JpegFrameStore *m_jpegStore = nullptr;

    QAtomicInt m_stopRequested {0};
};

#endif
```

## `jpegencoderworker.cpp`

```cpp
#include "jpegencoderworker.h"

#include "camera/framestore.h"
#include "jpegencoder.h"
#include "jpegframestore.h"

namespace {

constexpr int kJpegQuality = 85;
constexpr int kCaptureDivider = 2;

}

JpegEncoderWorker::JpegEncoderWorker(
        FrameStore *rawStore,
        JpegFrameStore *jpegStore,
        QObject *parent)
    : QThread(parent)
    , m_rawStore(rawStore)
    , m_jpegStore(jpegStore)
{
}

JpegEncoderWorker::~JpegEncoderWorker()
{
    requestStop();
    wait(5000);
}

void JpegEncoderWorker::requestStop()
{
    m_stopRequested.storeRelease(1);

    if (m_rawStore != nullptr)
        m_rawStore->wakeAll();
}

void JpegEncoderWorker::run()
{
    quint64 previousGeneration = 0;
    quint64 capturedFrames = 0;

    while (m_stopRequested.loadAcquire() == 0) {

        CameraFrame frame;

        if (!m_rawStore->waitForNewFrame(
                    previousGeneration,
                    100,
                    &frame)) {
            continue;
        }

        previousGeneration = frame.generation;

        ++capturedFrames;

        // CameraCapture 为 30 FPS。
        // 每两帧取一帧，统一生成约 15 FPS JPEG。
        if ((capturedFrames % kCaptureDivider) != 0)
            continue;

        QByteArray jpeg;
        QString error;

        if (!JpegEncoder::encode(
                    frame,
                    kJpegQuality,
                    &jpeg,
                    &error)) {

            emit encoderError(error);
            continue;
        }

        m_jpegStore->publish(
                    jpeg,
                    frame.width,
                    frame.height);
    }
}
```

此时数据流第一次真正变成：

```text
CameraCapture
      ↓
FrameStore RGB565
      ↓
JpegEncoderWorker
      ↓
JpegFrameStore
```

而且你原来的 `CameraCapture` 完全不用修改。

这是这套方案的一大优点。你当前采集线程自己独占 `/dev/video1`，因此绝对不要再搞一个 `v4l2src device=/dev/video1`。

------

# 五、让 AviRecorderWorker 不再接收 RGB565

现在才修改录像线程。

当前是：

```cpp
class FrameStore;

FrameStore *m_frameStore;
```

改成：

```cpp
class JpegFrameStore;

JpegFrameStore *m_jpegFrameStore;
```

构造函数从：

```cpp
AviRecorderWorker(FrameStore *frameStore,
                  ...)
```

改：

```cpp
AviRecorderWorker(JpegFrameStore *jpegFrameStore,
                  ...)
```

CPP：

```cpp
AviRecorderWorker::AviRecorderWorker(
        JpegFrameStore *jpegFrameStore,
        const QString &partPath,
        const QString &finalPath,
        QObject *parent)
    : QThread(parent)
    , m_jpegFrameStore(jpegFrameStore)
    , m_partPath(partPath)
    , m_finalPath(finalPath)
{
}
```

然后当前这三个东西：

```cpp
constexpr int kCaptureToRecordDivider = 2;

quint64 capturedFrames = 0;

if ((capturedFrames % kCaptureToRecordDivider) != 0)
    continue;
```

全部删除。

因为：

```text
30 → 15 FPS
```

已经归 `JpegEncoderWorker` 管。

你的 `run()` 最终应该接近：

```cpp
void AviRecorderWorker::run()
{
    if (m_stopRequested.loadAcquire() != 0) {
        emit recordingFinished(
                    false,
                    m_partPath,
                    QStringLiteral("录像在启动前已被停止"));
        return;
    }

    AviWriter writer;

    if (!writer.open(
                m_partPath,
                640,
                480,
                15,
                85)) {

        emit recordingFinished(
                    false,
                    m_partPath,
                    writer.errorString());
        return;
    }

    emit recordingStarted();

    quint64 previousGeneration = 0;

    QString failure;

    while (m_stopRequested.loadAcquire() == 0) {

        JpegFrame frame;

        if (!m_jpegFrameStore->waitForNewFrame(
                    previousGeneration,
                    100,
                    &frame)) {
            continue;
        }

        previousGeneration = frame.generation;

        if (!writer.appendJpegFrame(frame.bytes)) {
            failure = writer.errorString();
            break;
        }
    }

    if (!failure.isEmpty()) {
        writer.abort();

        emit recordingFinished(
                    false,
                    m_partPath,
                    failure);

        return;
    }

    if (!writer.finalize()) {
        emit recordingFinished(
                    false,
                    m_partPath,
                    writer.errorString());

        return;
    }

    if (QFile::exists(m_finalPath)
            || !QFile::rename(
                m_partPath,
                m_finalPath)) {

        emit recordingFinished(
                    false,
                    m_partPath,
                    QStringLiteral(
                        "AVI 已收尾，但临时文件改名失败"));

        return;
    }

    emit recordingFinished(
                true,
                m_finalPath,
                QStringLiteral("录像已保存，共 %1 帧")
                .arg(writer.frameCount()));
}
```

`requestStop()` 也改：

```cpp
void AviRecorderWorker::requestStop()
{
    m_stopRequested.storeRelease(1);

    if (m_jpegFrameStore != nullptr)
        m_jpegFrameStore->wakeAll();
}
```

到这里为止仍然不要写 RTSP。

再次测试录像。

如果此时 AVI 正常，说明最危险的重构已经完成。

------

# 六、这时候 RTSP 就变得非常简单了

现在 RTSP 根本不知道：

```text
/dev/video1
RGB565
libjpeg
AVI
```

它只知道：

```text
JpegFrameStore
     ↓
JPEG
```

所以 RTSP 管线就是：

```text
appsrc
   ↓
jpegparse
   ↓
rtpjpegpay
   ↓
GstRTSPServer
```

你的方案里这个 pipeline 是正确方向：

```text
appsrc
    name=jpegsrc
    is-live=true
    format=time
    block=false
!
image/jpeg,width=640,height=480,framerate=15/1
!
queue max-size-buffers=2 leaky=downstream
!
jpegparse
!
rtpjpegpay name=pay0 pt=26
```

`rtpjpegpay` 本身就是把 JPEG 图像封装为 RFC 2435 RTP/JPEG，并要求上游 caps 提供正确的宽高。([GStreamer](https://gstreamer.freedesktop.org/documentation/rtp/rtpjpegpay.html?utm_source=chatgpt.com))

而 `appsrc` 正是给“应用程序自己产生数据，再送进 GStreamer”使用的；你的 JPEG 就属于这种情况。([GStreamer](https://gstreamer.freedesktop.org/documentation/applib/gstappsrc.html?utm_source=chatgpt.com))

------

# 七、新增 RtspServerWorker

我建议这里直接做成：

```cpp
QThread
```

而不是把 `g_main_loop_run()` 塞进 `CameraController`。

结构：

```text
Qt GUI线程
    │
CameraController
    │
    ├── CameraCapture       QThread
    ├── JpegEncoderWorker   QThread
    ├── AviRecorderWorker   QThread
    └── RtspServerWorker    QThread
             ↓
        GLib MainLoop
```

GstRTSPServer 必须依赖 `GMainContext/MainLoop` 去接收连接。官方接口也是把 server attach 到 `GMainContext` 后由 main loop 调度。([GStreamer](https://gstreamer.freedesktop.org/documentation/gst-rtsp-server/rtsp-server-object.html?utm_source=chatgpt.com))

## `rtspserverworker.h`

核心可以这样写：

```cpp
#ifndef RTSPSERVERWORKER_H
#define RTSPSERVERWORKER_H

#include <QAtomicInt>
#include <QMutex>
#include <QThread>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/rtsp-server/rtsp-server.h>

class JpegFrameStore;

class RtspServerWorker : public QThread
{
    Q_OBJECT

public:
    explicit RtspServerWorker(
            JpegFrameStore *store,
            QObject *parent = nullptr);

    ~RtspServerWorker() override;

    void requestStop();

signals:
    void serverStarted();
    void serverStopped();
    void serverError(const QString &message);

protected:
    void run() override;

private:
    static void onMediaConfigure(
            GstRTSPMediaFactory *factory,
            GstRTSPMedia *media,
            gpointer userData);

    static void onMediaUnprepared(
            GstRTSPMedia *media,
            gpointer userData);

    static gboolean onPushTimer(gpointer userData);

    gboolean pushLatestFrame();

    JpegFrameStore *m_store = nullptr;

    QAtomicInt m_stopRequested {0};

    GMainContext *m_context = nullptr;
    GMainLoop *m_loop = nullptr;

    GstRTSPServer *m_server = nullptr;
    GstRTSPMediaFactory *m_factory = nullptr;

    GstElement *m_appsrc = nullptr;

    GSource *m_pushSource = nullptr;
    GSource *m_serverSource = nullptr;

    quint64 m_previousGeneration = 0;
    quint64 m_rtspFrameIndex = 0;

    QMutex m_loopMutex;
};

#endif
```

------

# 八、RTSP Server 的 run() 做什么

基本逻辑是：

```text
初始化 GStreamer
↓
创建 private GMainContext
↓
创建 GstRTSPServer
↓
端口 8554
↓
创建 MediaFactory
↓
设置 pipeline
↓
挂载 /camera
↓
启动 GLib MainLoop
```

核心代码：

```cpp
void RtspServerWorker::run()
{
    gst_init(nullptr, nullptr);

    m_context = g_main_context_new();
    m_loop = g_main_loop_new(m_context, FALSE);

    m_server = gst_rtsp_server_new();

    gst_rtsp_server_set_service(
                m_server,
                "8554");

    GstRTSPMountPoints *mounts =
            gst_rtsp_server_get_mount_points(
                m_server);

    m_factory =
            gst_rtsp_media_factory_new();

    const char *launch =
            "( "
            "appsrc name=jpegsrc "
            "is-live=true "
            "format=time "
            "block=false "
            "caps=\"image/jpeg,"
            "width=(int)640,"
            "height=(int)480,"
            "framerate=(fraction)15/1\" "
            "! queue "
            "max-size-buffers=2 "
            "leaky=downstream "
            "! jpegparse "
            "! rtpjpegpay "
            "name=pay0 "
            "pt=26 "
            ")";

    gst_rtsp_media_factory_set_launch(
                m_factory,
                launch);

    gst_rtsp_media_factory_set_shared(
                m_factory,
                TRUE);

    g_signal_connect(
                m_factory,
                "media-configure",
                G_CALLBACK(
                    RtspServerWorker::onMediaConfigure),
                this);

    gst_rtsp_mount_points_add_factory(
                mounts,
                "/camera",
                m_factory);

    g_object_unref(mounts);
```

这里：

```text
/camera
```

就是：

```text
rtsp://开发板IP:8554/camera
```

而：

```cpp
gst_rtsp_media_factory_set_shared(
            m_factory,
            TRUE);
```

表示多个客户端尽量共用同一个 media pipeline。

------

# 九、为什么必须叫 `pay0`

这一点很容易忽略。

你的 pipeline 最后：

```cpp
rtpjpegpay name=pay0 pt=26
```

这个：

```text
name=pay0
```

不是随便起名字。

GstRTSPServer 会根据：

```text
pay0
pay1
pay2
...
```

识别要发布出去的 RTP stream。

所以不能改成：

```text
name=myPayloader
```

否则 RTSP Server 不知道哪个 element 是 RTP 输出。

------

# 十、客户端连接时找到 appsrc

当 PotPlayer/VLC 请求：

```text
rtsp://192.168.x.x:8554/camera
```

GstRTSPServer 才真正根据 factory 创建：

```text
appsrc → jpegparse → rtpjpegpay
```

因此不能启动 RTSP Server 时直接找 `jpegsrc`。

要用：

```text
media-configure
```

官方 `GstRTSPMediaFactory` 就提供这个信号。([GStreamer](https://gstreamer.freedesktop.org/documentation/gst-rtsp-server/rtsp-media-factory.html?utm_source=chatgpt.com))

代码：

```cpp
void RtspServerWorker::onMediaConfigure(
        GstRTSPMediaFactory *,
        GstRTSPMedia *media,
        gpointer userData)
{
    RtspServerWorker *self =
            static_cast<RtspServerWorker *>(
                userData);

    GstElement *element =
            gst_rtsp_media_get_element(media);

    GstElement *appsrc =
            gst_bin_get_by_name_recurse_up(
                GST_BIN(element),
                "jpegsrc");

    gst_object_unref(element);

    if (appsrc == nullptr)
        return;

    if (self->m_appsrc != nullptr)
        gst_object_unref(self->m_appsrc);

    self->m_appsrc = appsrc;

    self->m_previousGeneration = 0;
    self->m_rtspFrameIndex = 0;

    g_signal_connect(
                media,
                "unprepared",
                G_CALLBACK(
                    RtspServerWorker::onMediaUnprepared),
                self);
}
```

客户端全部断开，media 被 unprepare 时把指针清掉：

```cpp
void RtspServerWorker::onMediaUnprepared(
        GstRTSPMedia *,
        gpointer userData)
{
    RtspServerWorker *self =
            static_cast<RtspServerWorker *>(
                userData);

    if (self->m_appsrc != nullptr) {
        gst_object_unref(self->m_appsrc);
        self->m_appsrc = nullptr;
    }

    self->m_previousGeneration = 0;
}
```

`GstRTSPMedia` 确实有 `unprepared` 信号，适合这里清理 media 相关对象。([GStreamer](https://gstreamer.freedesktop.org/documentation/gst-rtsp-server/rtsp-media.html?utm_source=chatgpt.com))

------

# 十一、怎么把 JPEG 真正塞进 appsrc

这里我建议第一版不要再增加一个 Qt 推流线程。

直接在 RTSP 的 GLib context 里放一个：

```text
20ms timer
```

每次检查：

```text
JpegFrameStore 有没有新 generation？
```

有就 push，没有就什么都不做。

创建：

```cpp
m_pushSource = g_timeout_source_new(20);

g_source_set_callback(
            m_pushSource,
            RtspServerWorker::onPushTimer,
            this,
            nullptr);

g_source_attach(
            m_pushSource,
            m_context);
```

callback：

```cpp
gboolean RtspServerWorker::onPushTimer(
        gpointer userData)
{
    RtspServerWorker *self =
            static_cast<RtspServerWorker *>(
                userData);

    return self->pushLatestFrame();
}
```

真正发送：

```cpp
gboolean RtspServerWorker::pushLatestFrame()
{
    if (m_stopRequested.loadAcquire() != 0)
        return G_SOURCE_REMOVE;

    if (m_appsrc == nullptr)
        return G_SOURCE_CONTINUE;

    const JpegFrame frame = m_store->latest();

    if (!frame.isValid())
        return G_SOURCE_CONTINUE;

    if (frame.generation == m_previousGeneration)
        return G_SOURCE_CONTINUE;

    m_previousGeneration = frame.generation;

    GstBuffer *buffer =
            gst_buffer_new_allocate(
                nullptr,
                static_cast<gsize>(
                    frame.bytes.size()),
                nullptr);

    if (buffer == nullptr)
        return G_SOURCE_CONTINUE;

    gst_buffer_fill(
                buffer,
                0,
                frame.bytes.constData(),
                static_cast<gsize>(
                    frame.bytes.size()));

    const GstClockTime duration =
            gst_util_uint64_scale_int(
                1,
                GST_SECOND,
                15);

    GST_BUFFER_PTS(buffer) =
            m_rtspFrameIndex * duration;

    GST_BUFFER_DTS(buffer) =
            GST_CLOCK_TIME_NONE;

    GST_BUFFER_DURATION(buffer) =
            duration;

    ++m_rtspFrameIndex;

    const GstFlowReturn ret =
            gst_app_src_push_buffer(
                GST_APP_SRC(m_appsrc),
                buffer);

    if (ret != GST_FLOW_OK
            && ret != GST_FLOW_FLUSHING) {
        // 第一版可以只打印日志。
    }

    return G_SOURCE_CONTINUE;
}
```

这里有个很重要的内存规则：

```cpp
gst_app_src_push_buffer()
```

直接 API 会接管传入的 `GstBuffer`，所以调用后不要：

```cpp
gst_buffer_unref(buffer);
```

GStreamer 官方教程也专门说明了 direct `gst_app_src_push_buffer()` 与 `push-buffer` signal 在 ownership 上的这个区别。([GStreamer](https://gstreamer.freedesktop.org/documentation/tutorials/basic/short-cutting-the-pipeline.html?utm_source=chatgpt.com))

------

# 十二、为什么 `queue leaky=downstream` 很重要

你的板子是实时监控，而不是视频转码服务器。

假设 Wi-Fi 突然卡了：

```text
JPEG编码 15FPS
       ↓
网络只能发 8FPS
```

如果普通 queue：

```text
1
2
3
4
5
6
...
```

会越来越积压。

最终用户看到：

```text
现在是12:00:10
屏幕还在播放12:00:06
```

不行。

所以：

```text
queue max-size-buffers=2 leaky=downstream
```

意思就是：

```text
队列最多保留很少的数据

网络跟不上
   ↓
丢旧 buffer
   ↓
优先最新 JPEG
```

这正好和你 `FrameStore` 的设计理念一致。

------

# 十三、完成 RTSP Server 的事件循环

前面的 server 配置完成后：

```cpp
GError *error = nullptr;

m_serverSource =
        gst_rtsp_server_create_source(
            m_server,
            nullptr,
            &error);

if (m_serverSource == nullptr) {

    QString message =
            QStringLiteral("创建 RTSP Server Source 失败");

    if (error != nullptr) {
        message += QStringLiteral("：")
                + QString::fromUtf8(error->message);

        g_error_free(error);
    }

    emit serverError(message);

    return;
}

g_source_attach(
            m_serverSource,
            m_context);

emit serverStarted();

g_main_loop_run(m_loop);
```

我这里比你原 Markdown 多做了一点：

没有简单写：

```cpp
gst_rtsp_server_attach(server, context);
```

而是显式：

```cpp
gst_rtsp_server_create_source()
g_source_attach()
```

原因是你使用的是自己创建的 private `GMainContext`。GStreamer 官方文档明确指出：对于非 default context，如果希望精确控制 source 生命周期，推荐 `gst_rtsp_server_create_source()` 再手动 attach。([GStreamer](https://gstreamer.freedesktop.org/documentation/gst-rtsp-server/rtsp-server-object.html?utm_source=chatgpt.com))

对你这种：

```text
RTSP可以开
RTSP可以关
程序还继续运行
```

的 Qt 应用更容易做干净的资源回收。

------

# 十四、CameraController 才是最后改的地方

你目前 `CameraController` 只有：

```cpp
FrameStore *m_frameStore;
CameraCapture *m_capture;
AviRecorderWorker *m_recorder;
```

而且 `startRecording()` 当前直接：

```cpp
new AviRecorderWorker(
    m_frameStore,
    ...
);
```

这是我们最后要换掉的地方。

增加：

```cpp
class JpegFrameStore;
class JpegEncoderWorker;
class RtspServerWorker;
```

成员：

```cpp
JpegFrameStore *m_jpegFrameStore = nullptr;

JpegEncoderWorker *m_jpegEncoder = nullptr;

RtspServerWorker *m_rtspServer = nullptr;

bool m_rtspEnabled = false;

QString m_rtspStatus;
```

再增加：

```cpp
Q_PROPERTY(bool rtspEnabled
           READ rtspEnabled
           NOTIFY rtspEnabledChanged)

Q_PROPERTY(QString rtspStatus
           READ rtspStatus
           NOTIFY rtspStatusChanged)
```

接口：

```cpp
bool rtspEnabled() const;
QString rtspStatus() const;

Q_INVOKABLE void startRtsp();
Q_INVOKABLE void stopRtsp();
```

signal：

```cpp
void rtspEnabledChanged();
void rtspStatusChanged();
```

------

# 十五、CameraController 构造时只创建 JpegFrameStore

例如：

```cpp
CameraController::CameraController(
        FrameStore *frameStore,
        QObject *parent)
    : QObject(parent)
    , m_frameStore(frameStore)
{
    m_jpegFrameStore =
            new JpegFrameStore();

    ...
}
```

当然更推荐把它做普通成员而不是裸指针，例如：

```cpp
JpegFrameStore m_jpegFrameStore;
```

这样不用手动 delete。

然后 encoder 采用“需要的时候创建”。

------

# 十六、JPEG Encoder 的生命周期非常关键

不能：

```text
打开摄像头
↓
JpegEncoderWorker 永久15FPS跑
```

否则 Mode1 纯预览也在疯狂做 JPEG 软件编码。

你真正需要的是：

```cpp
recording == true
||
rtspEnabled == true
```

才运行 JPEG Encoder。

写两个函数：

```cpp
void CameraController::startJpegEncoderIfNeeded()
{
    if (m_jpegEncoder != nullptr) {
        if (m_jpegEncoder->isRunning())
            return;

        delete m_jpegEncoder;
        m_jpegEncoder = nullptr;
    }

    m_jpegEncoder =
            new JpegEncoderWorker(
                m_frameStore,
                m_jpegFrameStore,
                this);

    connect(
        m_jpegEncoder,
        &JpegEncoderWorker::encoderError,
        this,
        [this](const QString &message) {
            setErrorMessage(message);
        });

    m_jpegEncoder->start();
}
```

停止：

```cpp
void CameraController::stopJpegEncoderIfUnused()
{
    if (m_recording
            || m_stopping
            || m_rtspEnabled) {
        return;
    }

    if (m_jpegEncoder == nullptr)
        return;

    m_jpegEncoder->requestStop();
    m_jpegEncoder->wait(3000);

    delete m_jpegEncoder;
    m_jpegEncoder = nullptr;

    m_jpegFrameStore->clear();
}
```

于是状态就非常漂亮：

```text
纯预览
CameraCapture
↓
FrameStore
↓
QML

没有 JPEG Encoder
```

录像：

```text
CameraCapture
↓
FrameStore
↓
JpegEncoder
↓
AVI
```

RTSP：

```text
CameraCapture
↓
FrameStore
↓
JpegEncoder
↓
RTSP
```

录像+RTSP：

```text
CameraCapture
↓
FrameStore
↓
JpegEncoder  ← 仍然只有一个
↓
JpegFrameStore
├── AVI
└── RTSP
```

这正是你上传方案的核心设计。

------

# 十七、startRecording() 要怎么改

你现在：

```cpp
m_recorder =
    new AviRecorderWorker(
        m_frameStore,
        partPath,
        finalPath,
        this);
```

改成：

```cpp
startJpegEncoderIfNeeded();

m_recorder =
    new AviRecorderWorker(
        m_jpegFrameStore,
        partPath,
        finalPath,
        this);
```

注意顺序：

```text
先 JPEG Encoder
↓
再 Recorder
```

这样 Recorder 启动后就等待：

```text
JpegFrameStore
```

而不是等 RGB565。

------

# 十八、录像结束时不能无脑关闭 JPEG Encoder

比如当前状态：

```text
RTSP ON
Recording ON
```

然后用户：

```text
stopRecording()
```

最终状态应该：

```text
RTSP ON
Recording OFF
JPEG Encoder ON
```

所以你现在的：

```cpp
onRecordingFinished()
```

最后加：

```cpp
stopJpegEncoderIfUnused();
```

它内部会判断：

```cpp
m_rtspEnabled
```

所以 RTSP 开着就不会停。

------

# 十九、startRtsp() 应该是什么逻辑

```cpp
void CameraController::startRtsp()
{
    if (!m_cameraOpen || m_cameraBusy) {
        setErrorMessage(
            QStringLiteral(
                "请先开启摄像头并等待预览画面"));
        return;
    }

    if (m_rtspEnabled)
        return;

    startJpegEncoderIfNeeded();

    m_rtspServer =
            new RtspServerWorker(
                m_jpegFrameStore,
                this);

    connect(
        m_rtspServer,
        &RtspServerWorker::serverStarted,
        this,
        [this]() {

            m_rtspEnabled = true;
            emit rtspEnabledChanged();

            m_rtspStatus =
                    QStringLiteral(
                        "RTSP 已开启：端口 8554，路径 /camera");

            emit rtspStatusChanged();
        });

    connect(
        m_rtspServer,
        &RtspServerWorker::serverError,
        this,
        [this](const QString &message) {

            setErrorMessage(message);

            m_rtspEnabled = false;
            emit rtspEnabledChanged();

            stopJpegEncoderIfUnused();
        });

    m_rtspServer->start();
}
```

------

# 二十、stopRtsp()

顺序应该：

```text
先 RTSP
↓
再决定要不要停 JPEG Encoder
```

例如：

```cpp
void CameraController::stopRtsp()
{
    if (m_rtspServer != nullptr) {

        m_rtspServer->requestStop();
        m_rtspServer->wait(3000);

        delete m_rtspServer;
        m_rtspServer = nullptr;
    }

    if (m_rtspEnabled) {
        m_rtspEnabled = false;
        emit rtspEnabledChanged();
    }

    m_rtspStatus =
            QStringLiteral("RTSP 已关闭");

    emit rtspStatusChanged();

    stopJpegEncoderIfUnused();
}
```

如果此时：

```text
Recording ON
```

那么：

```cpp
stopJpegEncoderIfUnused()
```

发现：

```cpp
m_recording == true
```

就什么都不会做。

所以：

```text
关 RTSP
不会影响录像
```

反过来也一样：

```text
停止录像
不会影响 RTSP
```

这就是这个架构最大的价值。

------

# 二十一、关闭摄像头时顺序也要改变

当前 `closeCamera()` 只考虑了录像。

以后还需要考虑 RTSP。

推荐：

```text
stopRtsp()
↓
stopJpegEncoder
↓
CameraCapture::requestStop()
```

最简单可以规定：

```cpp
if (m_rtspEnabled) {
    setErrorMessage(
        QStringLiteral(
            "请先关闭 RTSP，再关闭摄像头"));
    return;
}
```

第一版我反而建议这样。

因为用户操作关系更清楚：

```text
录像必须先停
RTSP必须先停
然后才能关摄像头
```

等以后接 PIR 自动控制，再改成自动联动关闭。

------

# 二十二、`.pro` 最终怎么改

你现在 `.pro` 只有 Qt、pthread 和 libjpeg，没有 GStreamer 开发库。

增加 include：

```qmake
INCLUDEPATH += \
    src \
    src/camera \
    src/codec \
    src/recorder \
    src/rtsp
```

增加：

```qmake
CONFIG += link_pkgconfig

PKGCONFIG += \
    gstreamer-1.0 \
    gstreamer-app-1.0 \
    gstreamer-rtsp-server-1.0
```

增加源文件：

```qmake
SOURCES += \
    src/codec/jpegencoder.cpp \
    src/codec/jpegframestore.cpp \
    src/codec/jpegencoderworker.cpp \
    src/rtsp/rtspserverworker.cpp
```

增加：

```qmake
HEADERS += \
    src/codec/jpegencoder.h \
    src/codec/jpegframestore.h \
    src/codec/jpegencoderworker.h \
    src/rtsp/rtspserverworker.h
```

你的：

```qmake
-ljpeg
```

继续保留。

------

# 二十三、QML 最后才改

你当前 QML 已经有：

```text
摄像头开关
Mode1
Mode2
开始录像
```

所以只需要在录像按钮附近增加一个：

```qml
Button {
    Layout.fillWidth: true

    text: cameraController.rtspEnabled
          ? qsTr("关闭 RTSP")
          : qsTr("开启 RTSP")

    enabled: cameraController.cameraOpen
             && !cameraController.cameraBusy

    onClicked: {
        if (cameraController.rtspEnabled)
            cameraController.stopRtsp()
        else
            cameraController.startRtsp()
    }
}
```

再加：

```qml
Label {
    Layout.fillWidth: true

    text: cameraController.rtspEnabled
          ? qsTr("RTSP：rtsp://开发板IP:8554/camera")
          : qsTr("RTSP：已关闭")

    color: cameraController.rtspEnabled
           ? "#4ade80"
           : "#93a1b2"

    wrapMode: Text.WrapAnywhere
}
```

你现在 QML 的 UI 结构已经足够，完全没必要为了 RTSP 重写页面。

------

# 二十四、在真正开始改之前，先做这组检查

开发板：

```bash
gst-inspect-1.0 appsrc

gst-inspect-1.0 rtpjpegpay

gst-inspect-1.0 jpegparse

gst-inspect-1.0 queue
```

你至少必须看到：

```text
appsrc
rtpjpegpay
queue
```

`rtpjpegpay` 是 RTP/JPEG 真正的打包器。([GStreamer](https://gstreamer.freedesktop.org/documentation/rtp/rtpjpegpay.html?utm_source=chatgpt.com))

如果：

```bash
gst-inspect-1.0 jpegparse
```

不存在，也不意味着整个方案废掉。

第一版可以尝试：

```text
appsrc
↓
image/jpeg
↓
queue
↓
rtpjpegpay
```

因为 `rtpjpegpay` 本身就会读取 JPEG header 来生成 RTP/JPEG 信息。([GStreamer](https://gstreamer.freedesktop.org/documentation/rtp/rtpjpegpay.html?utm_source=chatgpt.com))

Ubuntu 交叉编译环境还必须查：

```bash
source /opt/fsl-imx-x11/4.1.15-2.1.0/environment-setup-cortexa7hf-neon-poky-linux-gnueabi

echo "$SDKTARGETSYSROOT"

find "$SDKTARGETSYSROOT" -name 'gstappsrc.h'

find "$SDKTARGETSYSROOT" -name 'rtsp-server.h'

find "$SDKTARGETSYSROOT" -name 'gstreamer-app-1.0.pc'

find "$SDKTARGETSYSROOT" -name 'gstreamer-rtsp-server-1.0.pc'
```

这一点非常重要：**开发板有 `libgstrtspserver` 只代表程序能运行，不代表 Ubuntu SDK 能编译你的 C++ RTSP 代码。**

------

# 二十五、你现在实际应该做到哪一步

不要现在一口气把上面全改完。

最合理的开发顺序只有这一条：

1. `AviWriter::encodeJpeg()` → `JpegEncoder::encode()`，确认录像正常；接着加入 `JpegFrameStore + JpegEncoderWorker`，让 AVI 改吃共享 JPEG，再确认录像正常；最后才加入 `RtspServerWorker`，先测试“预览+RTSP”，然后测试“录像+RTSP”。

你真正的第一个里程碑不是：

```text
PotPlayer看到RTSP
```

而应该是：

```text
CameraCapture
      ↓
FrameStore
      ↓
JpegEncoderWorker
      ↓
JpegFrameStore
      ↓
AviRecorderWorker
      ↓
AVI正常

同时 CPU 与原录像版本基本同级
```

只有这个跑通以后，加 RTSP 才只是：

```text
JpegFrameStore
      ↓
appsrc
      ↓
rtpjpegpay
```

而不是同时调试“摄像头、JPEG、AVI、GStreamer、RTSP”五个问题。

你上传的方案最终画出的架构就是这个方向：保留现有采集和预览，把 JPEG 抽成共享层，然后 AVI 与 RTSP 成为两个独立消费者。

**所以你现在先不要写 `rtspserverworker.cpp`。第一步就是改 `jpegencoder.\* + aviwriter.\*`，编译后确保原 AVI 录像完全正常；接着再改 `JpegFrameStore + JpegEncoderWorker + AviRecorderWorker`。** 这两阶段完成后，RTSP 部分会简单很多。