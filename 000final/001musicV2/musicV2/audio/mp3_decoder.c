/*
 * mp3_decoder.c - libmpg123 封装（纯 C）
 *
 * 统一输出: 交错 S16 (MPG123_ENC_SIGNED_16), 采样率/声道数跟随文件
 * (常见 44100Hz / 2ch)。
 *
 * 如果开发板上没有 libmpg123 而是 libmad, 只需重写本文件(保持接口不变):
 *   - mp3_open:  读 MPEG 帧头得到采样率/声道数
 *   - mp3_read:  mad_frame_decode + mad_synth 输出 S16 交错
 *   - mp3_seek:  跳帧实现, 或从文件头按帧计数快速定位
 *   - mp3_duration: 帧计数估算 (frames * 1152 / rate)
 */
#include "mp3_decoder.h"

#include <mpg123.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Mp3Decoder
{
    mpg123_handle *mh;
    long rate;
    int channels;
    long long duration_ms; /* -1 = 尚未扫描 */

    char last_error[128];  /* 最近一次失败的描述, 成功操作后清空 */
    int  last_code;        /* 最近一次失败的返回码, 成功操作后置 0 */
};

/* 记录最近一次失败 (本工程单解码器串行使用, 无需加锁) */
static void save_error(Mp3Decoder *d, int code, const char *msg)
{
    if (!d)
        return;
    d->last_code = code;
    snprintf(d->last_error, sizeof(d->last_error), "%s",
             msg ? msg : "unknown error");
}

static void fill_errbuf(char *buf, size_t size, const char *msg)
{
    if (buf && size > 0)
        snprintf(buf, size, "%s", msg ? msg : "unknown error");
}

int mp3_global_init(void)
{
    return mpg123_init() == MPG123_OK ? 0 : -1;
}

void mp3_global_exit(void)
{
    mpg123_exit();
}

Mp3Decoder *mp3_open(const char *path, unsigned int *rate, int *channels,
                     char *errbuf, size_t errbuf_size)
{
    Mp3Decoder *d = calloc(1, sizeof(*d));
    if (!d) {
        fill_errbuf(errbuf, errbuf_size, "内存不足");
        return NULL;
    }

    int err = MPG123_OK;
    d->mh = mpg123_new(NULL, &err);
    if (!d->mh) {
        fill_errbuf(errbuf, errbuf_size, mpg123_plain_strerror(err));
        free(d);
        return NULL;
    }

    if (mpg123_open(d->mh, path) != MPG123_OK) {
        fill_errbuf(errbuf, errbuf_size, mpg123_strerror(d->mh));
        mpg123_delete(d->mh);
        free(d);
        return NULL;
    }

    long r = 0;
    int c = 0, enc = 0;
    if (mpg123_getformat(d->mh, &r, &c, &enc) != MPG123_OK) {
        fill_errbuf(errbuf, errbuf_size, mpg123_strerror(d->mh));
        mpg123_delete(d->mh);
        free(d);
        return NULL;
    }

    /* 固定输出 S16 交错, 避免 libmpg123 中途切换输出格式 */
    mpg123_format_none(d->mh);
    if (mpg123_format(d->mh, r, c, MPG123_ENC_SIGNED_16) != MPG123_OK) {
        fill_errbuf(errbuf, errbuf_size, mpg123_strerror(d->mh));
        mpg123_delete(d->mh);
        free(d);
        return NULL;
    }

    d->rate = r;
    d->channels = c;
    d->duration_ms = -1;

    if (errbuf && errbuf_size > 0)
        errbuf[0] = '\0';
    if (rate)
        *rate = (unsigned int)r;
    if (channels)
        *channels = c;
    return d;
}

long mp3_read(Mp3Decoder *d, void *pcm, unsigned long frames)
{
    size_t want = frames * (size_t)d->channels * 2; /* S16 */
    size_t done = 0;
    int r = MPG123_OK;

    /* 流中途切换采样率/声道时 mpg123_read 返回 MPG123_NEW_FORMAT,
     * 需要重新确认并锁定输出格式后继续读 */
    for (int tries = 0;; ++tries) {
        r = mpg123_read(d->mh, pcm, want, &done);
        if (r != MPG123_NEW_FORMAT)
            break;
        if (tries >= 4) {
            save_error(d, r, "NEW_FORMAT 反复出现, 放弃读取");
            return -1;
        }
        long nr = 0;
        int nc = 0, ne = 0;
        if (mpg123_getformat(d->mh, &nr, &nc, &ne) != MPG123_OK) {
            save_error(d, r, mpg123_strerror(d->mh));
            return -1;
        }
        mpg123_format_none(d->mh);
        mpg123_format(d->mh, nr, nc, MPG123_ENC_SIGNED_16);
    }

    if (r == MPG123_OK) {
        d->last_code = 0;
        d->last_error[0] = '\0';
        return (long)(done / ((size_t)d->channels * 2));
    }
    if (r == MPG123_DONE)
        return 0;
    save_error(d, r, mpg123_strerror(d->mh));
    return -1;
}

long long mp3_seek_ms(Mp3Decoder *d, long long ms)
{
    off_t target = (off_t)(ms * d->rate / 1000); /* 每声道样本数 */
    off_t pos = mpg123_seek(d->mh, target, SEEK_SET);
    if (pos < 0) {
        save_error(d, (int)pos, mpg123_strerror(d->mh));
        /* 失败时退回文件头 */
        if (mpg123_seek(d->mh, 0, SEEK_SET) >= 0) {
            d->last_code = 0;
            d->last_error[0] = '\0';
        }
        return 0;
    }
    d->last_code = 0;
    d->last_error[0] = '\0';
    return (long long)(pos * 1000 / d->rate);
}

long long mp3_duration_ms(Mp3Decoder *d)
{
    if (d->duration_ms < 0) {
        off_t len = 0;
        if (mpg123_scan(d->mh) == MPG123_OK) {
            len = mpg123_length(d->mh);
            d->last_code = 0;
            d->last_error[0] = '\0';
        } else {
            save_error(d, -1, mpg123_strerror(d->mh));
        }
        /* 注意: ARM 32 位上 off_t 是 32 位 long,
         * len(样本数, 如 328s*44100=1448万) * 1000 会溢出 32 位,
         * 必须先提升到 64 位再乘, 否则时长会算成 ~36s 之类的错误值 */
        d->duration_ms = (len > 0) ? ((long long)len * 1000 / d->rate) : 0;
    }
    return d->duration_ms;
}

const char *mp3_last_error(const Mp3Decoder *d)
{
    return d ? d->last_error : "no decoder";
}

int mp3_last_code(const Mp3Decoder *d)
{
    return d ? d->last_code : -1;
}

void mp3_close(Mp3Decoder *d)
{
    if (!d)
        return;
    mpg123_close(d->mh);
    mpg123_delete(d->mh);
    free(d);
}
