#include "jpegencoder.h"

#include "camera/framestore.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <setjmp.h>

#include <jpeglib.h>

namespace
{

    /**
     * @brief 扩展 libjpeg 错误管理器，使错误能够返回调用方而不终止进程
     */
    struct JpegErrorManager
    {
        jpeg_error_mgr base;
        jmp_buf jumpBuffer;
        char message[JMSG_LENGTH_MAX];
    };

    /**
     * @brief 保存一次 JPEG 编码的全部可变状态，确保 longjmp 后仍可安全清理
     */
    struct JpegEncodeState
    {
        jpeg_compress_struct compressor;
        JpegErrorManager errors;
        unsigned char *output = nullptr;
        unsigned char *rgbRow = nullptr;
        size_t outputSize = 0;
        bool compressorCreated = false;
    };

    /**
     * @brief 捕获 libjpeg 致命错误并跳回当前编码函数
     */
    void onJpegError(j_common_ptr common)
    {
        JpegErrorManager *manager = reinterpret_cast<JpegErrorManager *>(common->err);
        (*common->err->format_message)(common, manager->message);
        longjmp(manager->jumpBuffer, 1);
    }

    /**
     * @brief 把错误信息填入可选输出参数
     */
    void setError(QString *errorString, const QString &message)
    {
        if (errorString != nullptr)
            *errorString = message;
    }

} // namespace

/**
 * @brief 将一帧 RGB565 摄像头帧压缩为内存中的 JPEG
 *
 * 该编码器不依赖 AVI、不持有全局状态，可被录像、预览、RTSP 等模块复用
 */
bool JpegEncoder::encode(const CameraFrame &frame,
                         int quality,
                         QByteArray *jpeg,
                         QString *errorString)
{
    if (jpeg == nullptr)
    {
        setError(errorString, QStringLiteral("JPEG 输出缓冲区为空"));
        return false;
    }

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
        setError(errorString,
                 QStringLiteral("JPEG 编码失败：%1")
                     .arg(QString::fromLocal8Bit(state->errors.message)));
        delete state;
        return false;
    }

    jpeg_create_compress(&state->compressor);
    state->compressorCreated = true;
    jpeg_mem_dest(&state->compressor, &state->output, &state->outputSize);
    state->compressor.image_width = static_cast<JDIMENSION>(frame.width);
    state->compressor.image_height = static_cast<JDIMENSION>(frame.height);
    state->compressor.input_components = 3;
    state->compressor.in_color_space = JCS_RGB;
    jpeg_set_defaults(&state->compressor);
    jpeg_set_quality(&state->compressor, quality, TRUE);
    jpeg_start_compress(&state->compressor, TRUE);

    state->rgbRow = static_cast<unsigned char *>(
        std::malloc(static_cast<size_t>(frame.width) * 3U));
    if (state->rgbRow == nullptr)
    {
        jpeg_destroy_compress(&state->compressor);
        state->compressorCreated = false;
        std::free(state->output);
        setError(errorString, QStringLiteral("申请 JPEG 行缓冲区失败"));
        delete state;
        return false;
    }

    JSAMPROW rowPointer[1];
    while (state->compressor.next_scanline < state->compressor.image_height)
    {
        const uchar *source = reinterpret_cast<const uchar *>(frame.bytes.constData()) + static_cast<int>(state->compressor.next_scanline) * frame.bytesPerLine;
        uchar *destination = state->rgbRow;
        for (int x = 0; x < frame.width; ++x)
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
