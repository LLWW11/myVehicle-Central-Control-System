#include "cameracapture.h"

#include "framestore.h"

#include <QByteArray>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

namespace
{

    const char kCameraDevice[] = "/dev/video1";
    constexpr int kFrameWidth = 640;
    constexpr int kFrameHeight = 480;
    constexpr int kCaptureFps = 30;
    constexpr unsigned int kBufferCount = 3;

    /**
     * @brief 自动重试被信号中断的 ioctl 调用。
     * @param fd 设备文件描述符。
     * @param request ioctl 请求码。
     * @param argument ioctl 参数。
     * @return ioctl 的最终返回值。
     */
    int safeIoctl(int fd, unsigned long request, void *argument)
    {
        int result = -1;
        do
        {
            result = ::ioctl(fd, request, argument);
        } while (result < 0 && errno == EINTR);
        return result;
    }

}

CameraCapture::CameraCapture(FrameStore *frameStore, QObject *parent)
    : QThread(parent), m_frameStore(frameStore)
{
}

CameraCapture::~CameraCapture()
{
    requestStop();
    wait(2000);
}

void CameraCapture::requestStop()
{
    m_stopRequested.storeRelease(1); //
}

void CameraCapture::run()
{
    bool firstFrame = true;

    // 对象每次开启都会重新创建，不在此重置标志，避免吞掉启动瞬间的停止请求
    if (m_stopRequested.loadAcquire() != 0)
    {
        emit captureStopped();
        return;
    }
    if (!openDevice() || !configureDevice() || !mapBuffers() || !startStreaming())
    {
        cleanup();
        emit captureStopped();
        return;
    }

    while (m_stopRequested.loadAcquire() == 0)
    {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(m_fd, &readSet);

        // 200 ms 超时让线程能够及时观察停止标志
        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;
        const int ready = ::select(m_fd + 1, &readSet, nullptr, nullptr, &timeout);
        if (ready < 0)
        {
            if (errno == EINTR)
                continue;
            reportSystemError(QStringLiteral("等待摄像头帧"));
            break;
        }
        if (ready == 0)
            continue;
        if (!captureOneFrame(&firstFrame))
            break;
    }

    cleanup();
    emit captureStopped();
}

bool CameraCapture::openDevice()
{
    m_fd = open(kCameraDevice, O_RDWR | O_NONBLOCK);
    if (m_fd < 0)
    {
        reportSystemError(QStringLiteral("打开 /dev/video1"));
        return false;
    }

    v4l2_capability capability;
    std::memset(&capability, 0, sizeof(capability));
    if (safeIoctl(m_fd, VIDIOC_QUERYCAP, &capability) < 0)
    {
        reportSystemError(QStringLiteral("查询摄像头能力"));
        return false;
    }

    const quint32 capabilities = (capability.capabilities & V4L2_CAP_DEVICE_CAPS)
                                     ? capability.device_caps
                                     : capability.capabilities;
    if ((capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0 || (capabilities & V4L2_CAP_STREAMING) == 0)
    {
        emit captureError(QStringLiteral("/dev/video1 不支持视频采集或 MMAP 流模式"));
        return false;
    }
    return true;
}

bool CameraCapture::configureDevice()
{
    v4l2_format format;
    std::memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = kFrameWidth;
    format.fmt.pix.height = kFrameHeight;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    format.fmt.pix.field = V4L2_FIELD_ANY;

    if (safeIoctl(m_fd, VIDIOC_S_FMT, &format) < 0)
    {
        reportSystemError(QStringLiteral("设置摄像头格式"));
        return false;
    }
    if (format.fmt.pix.width != kFrameWidth || format.fmt.pix.height != kFrameHeight || format.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565)
    {
        emit captureError(QStringLiteral("摄像头未接受 640×480 RGB565 格式"));
        return false;
    }

    m_width = static_cast<int>(format.fmt.pix.width);
    m_height = static_cast<int>(format.fmt.pix.height);
    m_bytesPerLine = static_cast<int>(format.fmt.pix.bytesperline);
    if (m_bytesPerLine < m_width * 2)
        m_bytesPerLine = m_width * 2;
    v4l2_streamparm parameters;
    std::memset(&parameters, 0, sizeof(parameters));
    parameters.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (safeIoctl(m_fd, VIDIOC_G_PARM, &parameters) == 0 && (parameters.parm.capture.capability & V4L2_CAP_TIMEPERFRAME))
    {
        parameters.parm.capture.timeperframe.numerator = 1;
        parameters.parm.capture.timeperframe.denominator = kCaptureFps;
        if (safeIoctl(m_fd, VIDIOC_S_PARM, &parameters) < 0)
        {
            reportSystemError(QStringLiteral("设置摄像头帧率"));
            return false;
        }
    }
    return true;
}

bool CameraCapture::mapBuffers()
{
    v4l2_requestbuffers request;
    std::memset(&request, 0, sizeof(request));
    request.count = kBufferCount;
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory = V4L2_MEMORY_MMAP;
    if (safeIoctl(m_fd, VIDIOC_REQBUFS, &request) < 0)
    {
        reportSystemError(QStringLiteral("申请摄像头缓冲区"));
        return false;
    }
    if (request.count < 2)
    {
        emit captureError(QStringLiteral("摄像头返回的 MMAP 缓冲区数量不足"));
        return false;
    }

    m_buffers.reserve(static_cast<int>(request.count));
    for (unsigned int index = 0; index < request.count; ++index)
    {
        v4l2_buffer buffer;
        std::memset(&buffer, 0, sizeof(buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = index;
        if (safeIoctl(m_fd, VIDIOC_QUERYBUF, &buffer) < 0)
        {
            reportSystemError(QStringLiteral("查询摄像头缓冲区"));
            return false;
        }

        MappedBuffer mapped;
        mapped.length = buffer.length;
        mapped.address = ::mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE,
                                MAP_SHARED, m_fd, buffer.m.offset);
        if (mapped.address == MAP_FAILED)
        {
            mapped.address = nullptr;
            reportSystemError(QStringLiteral("映射摄像头缓冲区"));
            return false;
        }
        m_buffers.append(mapped);
    }

    for (int index = 0; index < m_buffers.size(); ++index)
    {
        v4l2_buffer buffer;
        std::memset(&buffer, 0, sizeof(buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = static_cast<unsigned int>(index);
        if (safeIoctl(m_fd, VIDIOC_QBUF, &buffer) < 0)
        {
            reportSystemError(QStringLiteral("摄像头缓冲区入队"));
            return false;
        }
    }
    return true;
}

bool CameraCapture::startStreaming()
{
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (safeIoctl(m_fd, VIDIOC_STREAMON, &type) < 0)
    {
        reportSystemError(QStringLiteral("开启摄像头数据流"));
        return false;
    }
    m_streaming = true;
    return true;
}

bool CameraCapture::captureOneFrame(bool *firstFrame)
{
    v4l2_buffer buffer;
    std::memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    if (safeIoctl(m_fd, VIDIOC_DQBUF, &buffer) < 0)
    {
        if (errno == EAGAIN)
            return true;
        reportSystemError(QStringLiteral("取出摄像头帧"));
        return false;
    }

    bool validBuffer = buffer.index < static_cast<unsigned int>(m_buffers.size());
    if (validBuffer)
    {
        const MappedBuffer &mapped = m_buffers.at(static_cast<int>(buffer.index));
        const int requiredSize = m_bytesPerLine * m_height;
        const int availableSize = static_cast<int>(mapped.length);
        const bool bytesUsedValid = buffer.bytesused == 0 || buffer.bytesused >= static_cast<unsigned int>(requiredSize);
        if (requiredSize <= availableSize && bytesUsedValid)
        {
            const QByteArray bytes(static_cast<const char *>(mapped.address), requiredSize);
            m_frameStore->publish(bytes, m_width, m_height, m_bytesPerLine);
            if (*firstFrame)
            {
                *firstFrame = false;
                emit captureStarted(m_width, m_height, m_bytesPerLine);
            }
        }
        else
        {
            emit captureError(QStringLiteral("摄像头帧数据小于 640×480 RGB565 所需大小"));
            validBuffer = false;
        }
    }

    if (safeIoctl(m_fd, VIDIOC_QBUF, &buffer) < 0)
    {
        reportSystemError(QStringLiteral("归还摄像头帧"));
        return false;
    }
    return validBuffer;
}

void CameraCapture::cleanup()
{
    if (m_fd >= 0 && m_streaming)
    {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        safeIoctl(m_fd, VIDIOC_STREAMOFF, &type);
        m_streaming = false;
    }

    for (const MappedBuffer &buffer : m_buffers)
    {
        if (buffer.address != nullptr && buffer.length > 0)
            ::munmap(buffer.address, buffer.length);
    }
    m_buffers.clear();

    if (m_fd >= 0)
    {
        ::close(m_fd);
        m_fd = -1;
    }
    if (m_frameStore != nullptr)
        m_frameStore->clear();
}

/**
 * @brief 记录最近错误并发送 captureError 信号。
 */
void CameraCapture::reportSystemError(const QString &operation)
{
    emit captureError(QStringLiteral("%1失败：%2")
                          .arg(operation, QString::fromLocal8Bit(std::strerror(errno))));
}
