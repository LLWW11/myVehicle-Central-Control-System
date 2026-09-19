#include "jpegencoder.h"

#include "camera/framestore.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <setjmp.h>

#include <jpeglib.h>

namespace
{

    struct JpegErrorManager
    {
        jpeg_error_mgr base;
        jmp_buf jumpBuffer;
        char message[JMSG_LENGTH_MAX];
    };

    struct JpegEncodeState
    {
        jpeg_compress_struct compressor;
        JpegErrorManager errors;
        unsigned char *output = nullptr;
        unsigned char *rgbRow = nullptr;
        size_t outputSize = 0;
        bool compressorCreated = false;
    };

    void onJpegError(j_common_ptr common) // 捕获 libjpeg 致命错误并跳回当前编码函数
    {
        JpegErrorManager *manager = reinterpret_cast<JpegErrorManager *>(common->err);
        (*common->err->format_message)(common, manager->message);
        longjmp(manager->jumpBuffer, 1);
    }

    void setError(QString *errorString, const QString &message) // 把错误信息填入可选输出参数
    {
        if (errorString != nullptr)
            *errorString = message;
    }

} // namespace

// RGB565 → JPEG，直接保存到QByteArray
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
