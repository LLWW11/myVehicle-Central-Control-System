#include "aviwriter.h"

#include "camera/framestore.h"

#include <QFile>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <setjmp.h>
#include <unistd.h>

#include <jpeglib.h>

namespace
{

    constexpr int kAviHeaderSize = 232;

    /**
     * @brief 向字节缓冲区写入小端 16 位整数。
     */
    void putLe16(uchar *destination, quint16 value)
    {
        destination[0] = static_cast<uchar>(value & 0xffU);
        destination[1] = static_cast<uchar>((value >> 8U) & 0xffU);
    }

    /**
     * @brief 向字节缓冲区写入小端 32 位整数。
     */
    void putLe32(uchar *destination, quint32 value)
    {
        destination[0] = static_cast<uchar>(value & 0xffU);
        destination[1] = static_cast<uchar>((value >> 8U) & 0xffU);
        destination[2] = static_cast<uchar>((value >> 16U) & 0xffU);
        destination[3] = static_cast<uchar>((value >> 24U) & 0xffU);
    }

    /**
     * @brief 向字节缓冲区写入四字符编码。
     */
    void putFourcc(uchar *destination, const char fourcc[5])
    {
        std::memcpy(destination, fourcc, 4);
    }

    /**
     * @brief 扩展 libjpeg 错误管理器，使错误能够返回调用方而不终止进程。
     */
    struct JpegErrorManager
    {
        jpeg_error_mgr base;
        jmp_buf jumpBuffer;
        char message[JMSG_LENGTH_MAX];
    };

    /**
     * @brief 保存一次 JPEG 编码的全部可变状态，确保 longjmp 后仍可安全清理。
     */
    struct JpegEncodeState
    {
        jpeg_compress_struct compressor;
        JpegErrorManager errors;
        unsigned char *output = nullptr;
        unsigned char *rgbRow = nullptr;
        // unsigned long outputSize = 0;
        size_t outputSize = 0;
        bool compressorCreated = false;
    };

    /**
     * @brief 捕获 libjpeg 致命错误并跳回当前编码函数。
     */
    void onJpegError(j_common_ptr common)
    {
        JpegErrorManager *manager = reinterpret_cast<JpegErrorManager *>(common->err);
        (*common->err->format_message)(common, manager->message);
        longjmp(manager->jumpBuffer, 1);
    }

} // namespace

/**
 * @brief 构造未打开文件的 AVI 写入器。
 */
AviWriter::AviWriter() = default;

/**
 * @brief 析构写入器；若尚未正常完成，只关闭文件。
 */
AviWriter::~AviWriter()
{
    abort();
}

/**
 * @brief 创建 AVI 临时文件并写入初始文件头。
 */
bool AviWriter::open(const QString &partPath, int width, int height,
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

    if (width <= 0 || height <= 0 || fps <= 0 || jpegQuality < 1 || jpegQuality > 100)
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

/**
 * @brief 将一帧 RGB565 数据压缩并写入 AVI。
 */
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
    if (!encodeJpeg(frame, &jpeg))
        return false;

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

/**
 * @brief 写入 idx1 索引、回填文件头并同步关闭文件。
 */
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
    if (fileSize < 8 || !patchLe32(4, static_cast<quint32>(fileSize - 8)) || !patchLe32(48, m_frames) || !patchLe32(60, m_maxFrameSize) || !patchLe32(140, m_frames) || !patchLe32(144, m_maxFrameSize) || !patchLe32(224, static_cast<quint32>(moviEnd - kAviHeaderSize + 4)))
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

/**
 * @brief 放弃当前录像并关闭文件，不写入成功收尾。
 */
void AviWriter::abort()
{
    if (m_file != nullptr)
    {
        std::fclose(m_file);
        m_file = nullptr;
    }
}

/**
 * @brief 获取最近一次失败原因。
 */
QString AviWriter::errorString() const
{
    return m_errorString;
}

/**
 * @brief 获取已经成功写入的帧数。
 */
quint32 AviWriter::frameCount() const
{
    return m_frames;
}

/**
 * @brief 根据当前视频参数构造 232 字节 AVI 头。
 */
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

/**
 * @brief 将 RGB565 摄像头帧压缩为内存中的 JPEG。
 */
bool AviWriter::encodeJpeg(const CameraFrame &frame, QByteArray *jpeg)
{
    if (jpeg == nullptr)
        return false;

    // 可变编码状态放在堆中，避免 longjmp 使自动变量的值变为不确定。
    JpegEncodeState *state = new JpegEncodeState();
    std::memset(&state->compressor, 0, sizeof(state->compressor));
    std::memset(&state->errors, 0, sizeof(state->errors));
    state->compressor.err = jpeg_std_error(&state->errors.base);
    state->errors.base.error_exit = onJpegError;

    if (setjmp(state->errors.jumpBuffer) != 0)
    {
        if (state->compressorCreated)
            jpeg_destroy_compress(&state->compressor);
        std::free(state->output);
        std::free(state->rgbRow);
        m_errorString = QStringLiteral("JPEG 编码失败：%1")
                            .arg(QString::fromLocal8Bit(state->errors.message));
        delete state;
        return false;
    }

    jpeg_create_compress(&state->compressor);
    state->compressorCreated = true;
    jpeg_mem_dest(&state->compressor, &state->output, &state->outputSize);
    state->compressor.image_width = static_cast<JDIMENSION>(m_width);
    state->compressor.image_height = static_cast<JDIMENSION>(m_height);
    state->compressor.input_components = 3;
    state->compressor.in_color_space = JCS_RGB;
    jpeg_set_defaults(&state->compressor);
    jpeg_set_quality(&state->compressor, m_jpegQuality, TRUE);
    jpeg_start_compress(&state->compressor, TRUE);

    state->rgbRow = static_cast<unsigned char *>(
        std::malloc(static_cast<size_t>(m_width) * 3U));
    if (state->rgbRow == nullptr)
    {
        jpeg_destroy_compress(&state->compressor);
        state->compressorCreated = false;
        std::free(state->output);
        m_errorString = QStringLiteral("申请 JPEG 行缓冲区失败");
        delete state;
        return false;
    }

    JSAMPROW rowPointer[1];
    while (state->compressor.next_scanline < state->compressor.image_height)
    {
        const uchar *source = reinterpret_cast<const uchar *>(frame.bytes.constData()) + static_cast<int>(state->compressor.next_scanline) * frame.bytesPerLine;
        uchar *destination = state->rgbRow;
        for (int x = 0; x < m_width; ++x)
        {
            const quint16 pixel = static_cast<quint16>(source[0] | (static_cast<quint16>(source[1]) << 8U));
            destination[0] = static_cast<uchar>((pixel >> 8U) & 0xf8U);
            destination[1] = static_cast<uchar>((pixel >> 3U) & 0xfcU);
            destination[2] = static_cast<uchar>((pixel << 3U) & 0xf8U);
            source += 2;
            destination += 3;
        }
        rowPointer[0] = state->rgbRow;
        jpeg_write_scanlines(&state->compressor, rowPointer, 1);
    }

    jpeg_finish_compress(&state->compressor);
    jpeg_destroy_compress(&state->compressor);
    state->compressorCreated = false;
    *jpeg = QByteArray(reinterpret_cast<const char *>(state->output),
                       static_cast<int>(state->outputSize));
    std::free(state->output);
    std::free(state->rgbRow);
    state->output = nullptr;
    state->rgbRow = nullptr;
    delete state;
    return true;
}

/**
 * @brief 写入精确数量的字节并统一记录错误。
 */
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

/**
 * @brief 跳转到指定文件偏移并写入一个小端 32 位整数。
 */
bool AviWriter::patchLe32(long offset, quint32 value)
{
    uchar bytes[4];
    putLe32(bytes, value);
    if (std::fseek(m_file, offset, SEEK_SET) != 0 || !writeBytes(bytes, sizeof(bytes)))
    {
        if (m_errorString.isEmpty())
            m_errorString = QStringLiteral("回填 AVI 文件头失败");
        return false;
    }
    return true;
}
