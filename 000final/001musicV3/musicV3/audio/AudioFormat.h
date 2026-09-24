#ifndef AUDIOFORMAT_H

#define AUDIOFORMAT_H

#include <QtGlobal>

// 仅S16类型PCM

struct AudioFormat

{

    enum class SampleFormat
    {

        SignedInt16 // PCM 样本的数值格式
    };

    int sampleRate = 0; // hz
    int channels = 0;   // 1：单声道，2：双声道
    SampleFormat sampleFormat = SampleFormat::SignedInt16;

    bool isValid() const
    {
        if (sampleRate > 0 &&
            (channels == 1 || channels == 2) &&
            sampleFormat == SampleFormat::SignedInt16)
            return true;
        return false;
    }

    int bytesPerFrame() const // 一帧PCM占用
    {
        if (!isValid())
            return 0;
        return channels * static_cast<int>(sizeof(qint16));
    }

    bool operator==(const AudioFormat &format1) const // 当前和format1的PCM格式是否一样
    {
        if (sampleRate == format1.sampleRate &&
            channels == format1.channels &&
            sampleFormat == format1.sampleFormat)
            return true;

        return false;
    }

    bool operator!=(const AudioFormat &format1) const
    {
        return !(*this == format1);
    }
};

#endif /* AUDIOFORMAT_H */