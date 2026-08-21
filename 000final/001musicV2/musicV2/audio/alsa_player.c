/*
 * alsa_player.c - ALSA PCM 输出封装（纯 C）
 *
 * 职责:
 *   - 按 MP3 解码后的采样率/声道数打开 ALSA 设备(默认 "default",
 *     可用环境变量 CERU_ALSA_DEV 覆盖, 如 "hw:0")
 *   - 交错 S16 样本写出, 带 underrun/suspend 恢复
 *   - seek 前丢弃未播数据并重新 prepare
 *
 * 注意: ESTRPIPE 需要 _GNU_SOURCE(glibc 下默认不可见)
 */
#define _GNU_SOURCE

#include "alsa_player.h"

#include <alsa/asoundlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct AlsaDevice {
    snd_pcm_t* pcm;
    unsigned int rate;
    int channels;
    int can_pause;   /* 硬件是否支持 snd_pcm_pause */
};

AlsaDevice* alsa_open(const char* dev, unsigned int rate, int channels)
{
    AlsaDevice* d = calloc(1, sizeof(*d));
    if (!d)
        return NULL;

    int err = snd_pcm_open(&d->pcm, dev, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "[alsa] open '%s' failed: %s\n",
                dev, snd_strerror(err));
        free(d);
        return NULL;
    }

    snd_pcm_hw_params_t* hw;
    snd_pcm_hw_params_alloca(&hw);
    if ((err = snd_pcm_hw_params_any(d->pcm, hw)) < 0) goto fail;
    if ((err = snd_pcm_hw_params_set_access(d->pcm, hw,
             SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) goto fail;
    if ((err = snd_pcm_hw_params_set_format(d->pcm, hw,
             SND_PCM_FORMAT_S16_LE)) < 0) goto fail;
    if ((err = snd_pcm_hw_params_set_channels(d->pcm, hw,
             (unsigned int)channels)) < 0) goto fail;

    unsigned int r = rate;
    if ((err = snd_pcm_hw_params_set_rate_near(d->pcm, hw, &r, 0)) < 0)
        goto fail;
    if (r != rate) {
        fprintf(stderr, "[alsa] rate %u not supported (got %u)\n", rate, r);
        err = -EINVAL;
        goto fail;
    }

    snd_pcm_uframes_t buf_frames = 16384;   /* ~370ms @44.1kHz */
    snd_pcm_uframes_t per_frames = 4096;    /* ~93ms */
    snd_pcm_hw_params_set_buffer_size_near(d->pcm, hw, &buf_frames);
    snd_pcm_hw_params_set_period_size_near(d->pcm, hw, &per_frames, 0);
    if ((err = snd_pcm_hw_params(d->pcm, hw)) < 0) goto fail;

    /* sw params: 缓冲写一半再启动, 减少开头毛刺 */
    snd_pcm_sw_params_t* sw;
    snd_pcm_sw_params_alloca(&sw);
    snd_pcm_sw_params_current(d->pcm, sw);
    snd_pcm_sw_params_set_start_threshold(d->pcm, sw, buf_frames / 2);
    snd_pcm_sw_params_set_avail_min(d->pcm, sw, per_frames);
    snd_pcm_sw_params_set_stop_threshold(d->pcm, sw, buf_frames);
    snd_pcm_sw_params(d->pcm, sw);

    d->rate = r;
    d->channels = channels;
    d->can_pause = snd_pcm_hw_params_can_pause(hw);
    return d;

fail:
    fprintf(stderr, "[alsa] hw params failed: %s\n", snd_strerror(err));
    snd_pcm_close(d->pcm);
    free(d);
    return NULL;
}

long alsa_write(AlsaDevice* d, const void* data, unsigned long frames)
{
    snd_pcm_sframes_t f = snd_pcm_writei(d->pcm, data, frames);
    if (f == -EPIPE) {          /* underrun: 重新准备后重写一次 */
        snd_pcm_prepare(d->pcm);
        f = snd_pcm_writei(d->pcm, data, frames);
    }
#ifdef ESTRPIPE
    else if (f == -ESTRPIPE) {  /* suspend: 等设备恢复 */
        while ((f = snd_pcm_resume(d->pcm)) == -EAGAIN)
            usleep(1000);
        if (f < 0) {
            snd_pcm_prepare(d->pcm);
            f = snd_pcm_writei(d->pcm, data, frames);
        }
    }
#endif
    return (long)f;
}

int alsa_pause(AlsaDevice* d)
{
    if (!d->can_pause)
        return 0;
    return snd_pcm_pause(d->pcm, 1) == 0;
}

int alsa_resume(AlsaDevice* d)
{
    if (!d->can_pause)
        return 0;
    return snd_pcm_pause(d->pcm, 0) == 0;
}

void alsa_drop_and_prepare(AlsaDevice* d)
{
    snd_pcm_drop(d->pcm);
    snd_pcm_prepare(d->pcm);
}

void alsa_close(AlsaDevice* d)
{
    if (!d)
        return;
    if (d->pcm) {
        snd_pcm_drain(d->pcm);   /* 自然结束时把缓冲里的数据播完 */
        snd_pcm_close(d->pcm);
    }
    free(d);
}
