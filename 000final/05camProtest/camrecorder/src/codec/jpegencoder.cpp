#include "jpegencoder.h"

#include "camera/framestore.h"

#include <QFile>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <setjmp.h>
#include <unistd.h>

#include <jpeglib.h>

bool JpegEncoder::encode(const CameraFrame &frame,
                         int quality,
                         QByteArray *jpeg,
                         QString *errorString = nullptr)
{
    if (jpeg == nullptr)
        return false;
    ::JpegEncodeState *state = new JpegEncodeState();
    memset(jpgencstate->);
}