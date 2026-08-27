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