#include "aviwriter.h"

#include "camera/framestore.h"
#include "jpegencoder.h"

#include <QFile>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace
{

    constexpr int kAviHeaderSize = 232;

    void putLe16(uchar *destination, quint16 value)
    {
        destination[0] = static_cast<uchar>(value & 0xffU);
        destination[1] = static_cast<uchar>((value >> 8U) & 0xffU);
    }
    void putLe32(uchar *destination, quint32 value)
    {
        destination[0] = static_cast<uchar>(value & 0xffU);
        destination[1] = static_cast<uchar>((value >> 8U) & 0xffU);
        destination[2] = static_cast<uchar>((value >> 16U) & 0xffU);
        destination[3] = static_cast<uchar>((value >> 24U) & 0xffU);
    }
    void putFourcc(uchar *destination, const char fourcc[5])
    {
        std::memcpy(destination, fourcc, 4);
    }

} // namespace

AviWriter::AviWriter() = default;

AviWriter::~AviWriter()
{
    abort();
}

bool AviWriter::open(const QString &partPath,
                     int width, int height,
                     int fps, int jpegQuality)
{
    abort();
    m_errorString.clear();
    m_partPath = partPath;
    m_width = width;
    m_height = height;
    m_fps = fps;
    m_jpegQuality = jpegQuality;
    m_frames = 0;
    m_maxFrameSize = 0;
    m_chunkOffset = 4;
    m_index.clear();

    if (width <= 0 || height <= 0 || fps <= 0 ||
        jpegQuality < 1 || jpegQuality > 100)
    {
        m_errorString = QStringLiteral("AVI 参数无效");
        return false;
    }

    const QByteArray nativePath = QFile::encodeName(partPath);
    m_file = std::fopen(nativePath.constData(), "wb");
    if (m_file == nullptr)
    {
        m_errorString = QStringLiteral("创建录像文件失败：%1")
                            .arg(QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }

    const QByteArray header = buildHeader();
    if (!writeBytes(header.constData(), static_cast<size_t>(header.size())))
    {
        abort();
        return false;
    }
    return true;
}

bool AviWriter::appendFrame(const CameraFrame &frame)
{
    if (m_file == nullptr)
    {
        m_errorString = QStringLiteral("AVI 文件尚未打开");
        return false;
    }
    if (!frame.isValid() || frame.width != m_width || frame.height != m_height)
    {
        m_errorString = QStringLiteral("录像帧尺寸或数据无效");
        return false;
    }

    QByteArray jpeg;
    QString error;
    if (!JpegEncoder::encode(frame, m_jpegQuality, &jpeg, &error))
    {
        m_errorString = error;
        return false;
    }
    return appendJpegFrame(jpeg);
}

bool AviWriter::appendJpegFrame(const QByteArray &jpeg)
{
    if (m_file == nullptr)
    {
        m_errorString = QStringLiteral("AVI 文件尚未打开");
        return false;
    }
    if (jpeg.size() < 4 || static_cast<unsigned char>(jpeg[0]) != 0xff || static_cast<unsigned char>(jpeg[1]) != 0xd8)
    {
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
    if (!writeBytes(chunkHeader, sizeof(chunkHeader)) || !writeBytes(jpeg.constData(), static_cast<size_t>(jpeg.size())))
    {
        m_index.removeLast();
        return false;
    }

    m_chunkOffset += 8U + entry.size;
    if ((entry.size & 1U) != 0U)
    {
        const uchar padding = 0;
        if (!writeBytes(&padding, 1))
        {
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

bool AviWriter::finalize()
{
    if (m_file == nullptr)
    {
        m_errorString = QStringLiteral("没有可收尾的 AVI 文件");
        return false;
    }
    if (m_frames == 0)
    {
        m_errorString = QStringLiteral("录像中没有写入有效视频帧");
        abort();
        return false;
    }

    const long moviEnd = std::ftell(m_file);
    if (moviEnd < 0)
    {
        m_errorString = QStringLiteral("读取 AVI 写入位置失败");
        abort();
        return false;
    }

    uchar indexHeader[8];
    putFourcc(indexHeader, "idx1");
    putLe32(indexHeader + 4, static_cast<quint32>(m_index.size() * 16));
    if (!writeBytes(indexHeader, sizeof(indexHeader)))
    {
        abort();
        return false;
    }

    for (const IndexEntry &entry : m_index)
    {
        uchar indexData[16];
        putFourcc(indexData, "00dc");
        putLe32(indexData + 4, 0x10U);
        putLe32(indexData + 8, entry.offset);
        putLe32(indexData + 12, entry.size);
        if (!writeBytes(indexData, sizeof(indexData)))
        {
            abort();
            return false;
        }
    }

    if (std::fflush(m_file) != 0)
    {
        m_errorString = QStringLiteral("刷新 AVI 数据失败：%1")
                            .arg(QString::fromLocal8Bit(std::strerror(errno)));
        abort();
        return false;
    }
    const long fileSize = std::ftell(m_file);
    if (fileSize < 8 ||
        !patchLe32(4, static_cast<quint32>(fileSize - 8)) ||
        !patchLe32(48, m_frames) ||
        !patchLe32(60, m_maxFrameSize) ||
        !patchLe32(140, m_frames) ||
        !patchLe32(144, m_maxFrameSize) ||
        !patchLe32(224, static_cast<quint32>(moviEnd - kAviHeaderSize + 4)))
    {
        abort();
        return false;
    }

    if (std::fflush(m_file) != 0 || ::fsync(::fileno(m_file)) != 0)
    {
        m_errorString = QStringLiteral("同步 AVI 文件失败：%1")
                            .arg(QString::fromLocal8Bit(std::strerror(errno)));
        abort();
        return false;
    }
    if (std::fclose(m_file) != 0)
    {
        m_file = nullptr;
        m_errorString = QStringLiteral("关闭 AVI 文件失败：%1")
                            .arg(QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    m_file = nullptr;
    return true;
}

void AviWriter::abort()
{
    if (m_file != nullptr)
    {
        std::fclose(m_file);
        m_file = nullptr;
    }
}

QString AviWriter::errorString() const
{
    return m_errorString;
}

quint32 AviWriter::frameCount() const
{
    return m_frames;
}

QByteArray AviWriter::buildHeader() const
{
    QByteArray header(kAviHeaderSize, '\0');
    uchar *data = reinterpret_cast<uchar *>(header.data());

    putFourcc(data + 0, "RIFF");
    putFourcc(data + 8, "AVI ");
    putFourcc(data + 12, "LIST");
    putLe32(data + 16, 200);
    putFourcc(data + 20, "hdrl");
    putFourcc(data + 24, "avih");
    putLe32(data + 28, 56);
    putLe32(data + 32, static_cast<quint32>(1000000 / m_fps));
    putLe32(data + 44, 0x10U);
    putLe32(data + 56, 1);
    putLe32(data + 64, static_cast<quint32>(m_width));
    putLe32(data + 68, static_cast<quint32>(m_height));
    putFourcc(data + 88, "LIST");
    putLe32(data + 92, 124);
    putFourcc(data + 96, "strl");
    putFourcc(data + 100, "strh");
    putLe32(data + 104, 64);
    putFourcc(data + 108, "vids");
    putFourcc(data + 112, "MJPG");
    putLe32(data + 128, 1);
    putLe32(data + 132, static_cast<quint32>(m_fps));
    putLe32(data + 148, 0xffffffffU);
    putLe32(data + 164, static_cast<quint32>(m_width));
    putLe32(data + 168, static_cast<quint32>(m_height));
    putFourcc(data + 172, "strf");
    putLe32(data + 176, 40);
    putLe32(data + 180, 40);
    putLe32(data + 184, static_cast<quint32>(m_width));
    putLe32(data + 188, static_cast<quint32>(-m_height));
    putLe16(data + 192, 1);
    putLe16(data + 194, 24);
    putFourcc(data + 196, "MJPG");
    putFourcc(data + 220, "LIST");
    putFourcc(data + 228, "movi");
    return header;
}

bool AviWriter::writeBytes(const void *data, size_t size)
{
    if (m_file == nullptr || std::fwrite(data, 1, size, m_file) != size)
    {
        m_errorString = QStringLiteral("写入 AVI 文件失败：%1")
                            .arg(QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    return true;
}

bool AviWriter::patchLe32(long offset, quint32 value)
{
    uchar bytes[4];
    putLe32(bytes, value);
    if (std::fseek(m_file, offset, SEEK_SET) != 0 ||
        !writeBytes(bytes, sizeof(bytes)))
    {
        if (m_errorString.isEmpty())
            m_errorString = QStringLiteral("回填 AVI 文件头失败");
        return false;
    }
    return true;
}
