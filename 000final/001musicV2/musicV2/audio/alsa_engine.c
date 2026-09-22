/*
 * alsa_engine.c - 播放控制循环（纯 C）
 *
 * 线程模型:
 *   - engine_play 创建一条播放线程: 线程内 mp3 解码 -> 软件音量 -> ALSA 写出
 *   - 暂停/继续/跳转/停止 通过 互斥锁 + 条件变量 + 标志位 与播放线程通信
 *   - 所有 snd_pcm 调用只发生在播放线程, 避免并发访问设备
 *   - 暂停是"软件暂停": 停止读取/写出, 保留解码进度, 继续时无缝衔接
 *     (IMX6U 等板子内核常不支持 snd_pcm_pause, 统一用软件方式)
 *
 * 注意: 状态/错误/结束回调运行在播放线程上, 回调内禁止调用引擎接口,
 *       回调只做通知(如 emit Qt 信号, 自动跨线程排队)这类非阻塞动作。
 */
#include "alsa_engine.h"

#include "alsa_player.h"
#include "mp3_decoder.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ENGINE_PERIOD_FRAMES 4096   /* 每次写出帧数 (~93ms @44.1kHz), 越小控制响应越快 */
#define ENGINE_FILE_MAX      1024

typedef struct {
    pthread_t thread;
    int thread_running;

    pthread_mutex_t lock;
    pthread_cond_t  cond;

    int stop_req;
    int pause_flag;
    int seek_req;
    long long seek_ms;

    int state;            /* ENGINE_IDLE/PLAYING/PAUSED/ERROR */
    int volume;           /* 0-100 */
    long long position;   /* 已播放样本帧数 */
    long long duration;   /* 样本帧数, 0 = 未知 */
    unsigned int rate;
    int channels;

    char file[ENGINE_FILE_MAX];
    char last_error[256];

    engine_state_cb state_cb;
    engine_error_cb error_cb;
    engine_end_cb   end_cb;
    void* user;
} Engine;

static Engine g_engine;
static int    g_mp3_inited = 0;

static void notify_state(int state)
{
    if (g_engine.state_cb)
        g_engine.state_cb(state, g_engine.user);
}

static void engine_set_error(const char* msg)
{
    strncpy(g_engine.last_error, msg, sizeof(g_engine.last_error) - 1);
    g_engine.last_error[sizeof(g_engine.last_error) - 1] = '\0';
    if (g_engine.error_cb)
        g_engine.error_cb(msg, g_engine.user);
}

static void apply_volume(int16_t* buf, size_t samples, int volume)
{
    if (volume >= 100)
        return;
    float g = (float)volume / 100.0f;
    for (size_t i = 0; i < samples; ++i) {
        float s = buf[i] * g;
        if (s > 32767.0f)      s = 32767.0f;
        else if (s < -32768.0f) s = -32768.0f;
        buf[i] = (int16_t)s;
    }
}

static void* play_thread(void* arg)
{
    (void)arg;

    unsigned int rate = 0;
    int channels = 0;

    char errbuf[128];
    Mp3Decoder* dec = mp3_open(g_engine.file, &rate, &channels,
                               errbuf, sizeof(errbuf));
    if (!dec) {
        char msg[192];
        snprintf(msg, sizeof(msg), "[mp3] 打开文件/初始化解码器失败: %s", errbuf);
        engine_set_error(msg);
        pthread_mutex_lock(&g_engine.lock);
        g_engine.state = ENGINE_ERROR;
        g_engine.thread_running = 0;
        pthread_mutex_unlock(&g_engine.lock);
        notify_state(ENGINE_ERROR);
        notify_state(ENGINE_IDLE);
        return NULL;
    }

    long long dur_ms = mp3_duration_ms(dec);   /* 首次会扫描文件 */
    mp3_seek_ms(dec, 0);   /* scan 后强制复位到文件头, 避免残留状态影响首次读取 */

    pthread_mutex_lock(&g_engine.lock);
    g_engine.rate = rate;
    g_engine.channels = channels;
    g_engine.position = 0;
    g_engine.duration = dur_ms > 0 ? dur_ms * rate / 1000 : 0;
    pthread_mutex_unlock(&g_engine.lock);

    const char* dev = getenv("CERU_ALSA_DEV");
    if (!dev || !*dev)
        dev = "default";

    AlsaDevice* alsa = alsa_open(dev, rate, channels);
    if (!alsa) {
        mp3_close(dec);
        engine_set_error("[alsa] 打开音频设备失败(检查 CERU_ALSA_DEV/aplay -l)");
        pthread_mutex_lock(&g_engine.lock);
        g_engine.state = ENGINE_ERROR;
        g_engine.thread_running = 0;
        pthread_mutex_unlock(&g_engine.lock);
        notify_state(ENGINE_ERROR);
        notify_state(ENGINE_IDLE);
        return NULL;
    }

    int16_t* buf = malloc(ENGINE_PERIOD_FRAMES * (size_t)channels * 2);
    if (!buf) {
        alsa_close(alsa);
        mp3_close(dec);
        engine_set_error("[engine] 内存不足");
        pthread_mutex_lock(&g_engine.lock);
        g_engine.state = ENGINE_ERROR;
        g_engine.thread_running = 0;
        pthread_mutex_unlock(&g_engine.lock);
        notify_state(ENGINE_ERROR);
        notify_state(ENGINE_IDLE);
        return NULL;
    }

    pthread_mutex_lock(&g_engine.lock);
    g_engine.state = ENGINE_PLAYING;
    pthread_mutex_unlock(&g_engine.lock);
    notify_state(ENGINE_PLAYING);

    int result = 0;      /* 0=自然结束 1=停止 -1=错误 */
    int was_paused = 0;

    for (;;) {
        pthread_mutex_lock(&g_engine.lock);

        /* 暂停时挂起, 等待继续/停止 */
        while (!g_engine.stop_req && g_engine.pause_flag) {
            g_engine.state = ENGINE_PAUSED;
            was_paused = 1;
            pthread_cond_wait(&g_engine.cond, &g_engine.lock);
        }

        if (g_engine.stop_req) {
            pthread_mutex_unlock(&g_engine.lock);
            result = 1;
            break;
        }

        if (was_paused) {          /* 刚被唤醒恢复 */
            was_paused = 0;
            g_engine.state = ENGINE_PLAYING;
            pthread_mutex_unlock(&g_engine.lock);
            notify_state(ENGINE_PLAYING);
            continue;
        }

        if (g_engine.seek_req) {   /* 跳转 */
            g_engine.seek_req = 0;
            long long ms = g_engine.seek_ms;
            pthread_mutex_unlock(&g_engine.lock);

            long long real_ms = mp3_seek_ms(dec, ms);
            alsa_drop_and_prepare(alsa);   /* 清掉缓冲里的旧音频 */

            pthread_mutex_lock(&g_engine.lock);
            g_engine.position = real_ms * (long long)rate / 1000;
            pthread_mutex_unlock(&g_engine.lock);
            continue;
        }

        pthread_mutex_unlock(&g_engine.lock);

        /* 解码一段 */
        long n = mp3_read(dec, buf, ENGINE_PERIOD_FRAMES);
        if (n < 0) {
            char msg[256];
            snprintf(msg, sizeof(msg), "[mp3] 解码失败: %s (code=%d)",
                     mp3_last_error(dec), mp3_last_code(dec));
            engine_set_error(msg);
            result = -1;
            break;
        }
        if (n == 0)
            break;                 /* EOF, 自然结束 */

        pthread_mutex_lock(&g_engine.lock);
        int vol = g_engine.volume;
        pthread_mutex_unlock(&g_engine.lock);
        apply_volume(buf, (size_t)n * (size_t)channels, vol);

        if (alsa_write(alsa, buf, (unsigned long)n) < 0) {
            engine_set_error("[alsa] 写设备失败");
            result = -1;
            break;
        }

        pthread_mutex_lock(&g_engine.lock);
        g_engine.position += n;
        pthread_mutex_unlock(&g_engine.lock);
    }

    free(buf);
    alsa_close(alsa);
    mp3_close(dec);

    if (result == -1) {
        pthread_mutex_lock(&g_engine.lock);
        g_engine.state = ENGINE_ERROR;
        pthread_mutex_unlock(&g_engine.lock);
        notify_state(ENGINE_ERROR);
    }

    int do_end = (result == 0) && g_engine.end_cb;

    pthread_mutex_lock(&g_engine.lock);
    g_engine.thread_running = 0;
    g_engine.state = ENGINE_IDLE;
    pthread_mutex_unlock(&g_engine.lock);

    if (do_end)
        g_engine.end_cb(g_engine.user);
    notify_state(ENGINE_IDLE);
    return NULL;
}

int engine_init(engine_state_cb scb, engine_error_cb ecb,
                engine_end_cb endcb, void* user)
{
    memset(&g_engine, 0, sizeof(g_engine));
    pthread_mutex_init(&g_engine.lock, NULL);
    pthread_cond_init(&g_engine.cond, NULL);
    g_engine.state_cb = scb;
    g_engine.error_cb = ecb;
    g_engine.end_cb = endcb;
    g_engine.user = user;
    g_engine.state = ENGINE_IDLE;
    g_engine.volume = 100;
    g_engine.last_error[0] = '\0';

    if (!g_mp3_inited) {
        if (mp3_global_init() != 0)
            return -1;
        g_mp3_inited = 1;
    }
    return 0;
}

void engine_deinit(void)
{
    engine_stop();
    if (g_mp3_inited) {
        mp3_global_exit();
        g_mp3_inited = 0;
    }
    pthread_mutex_destroy(&g_engine.lock);
    pthread_cond_destroy(&g_engine.cond);
}

int engine_play(const char* path)
{
    if (!path || !*path)
        return -1;

    pthread_mutex_lock(&g_engine.lock);
    if (g_engine.thread_running) {      /* 上一首还在播, 先停 */
        g_engine.stop_req = 1;
        g_engine.pause_flag = 0;
        pthread_cond_broadcast(&g_engine.cond);
        pthread_t old = g_engine.thread;
        pthread_mutex_unlock(&g_engine.lock);
        pthread_join(old, NULL);
        pthread_mutex_lock(&g_engine.lock);
    }

    strncpy(g_engine.file, path, sizeof(g_engine.file) - 1);
    g_engine.file[sizeof(g_engine.file) - 1] = '\0';
    g_engine.stop_req = 0;
    g_engine.pause_flag = 0;
    g_engine.seek_req = 0;
    g_engine.position = 0;
    g_engine.duration = 0;
    g_engine.rate = 0;
    g_engine.state = ENGINE_IDLE;

    g_engine.thread_running = 1;
    if (pthread_create(&g_engine.thread, NULL, play_thread, NULL) != 0) {
        g_engine.thread_running = 0;
        pthread_mutex_unlock(&g_engine.lock);
        return -1;
    }
    pthread_mutex_unlock(&g_engine.lock);
    return 0;
}

int engine_pause(void)
{
    pthread_mutex_lock(&g_engine.lock);
    if (g_engine.state != ENGINE_PLAYING) {
        pthread_mutex_unlock(&g_engine.lock);
        return -1;
    }
    g_engine.pause_flag = 1;
    pthread_cond_broadcast(&g_engine.cond);
    pthread_mutex_unlock(&g_engine.lock);
    return 0;
}

int engine_resume(void)
{
    pthread_mutex_lock(&g_engine.lock);
    if (g_engine.state != ENGINE_PAUSED) {
        pthread_mutex_unlock(&g_engine.lock);
        return -1;
    }
    g_engine.pause_flag = 0;
    pthread_cond_broadcast(&g_engine.cond);
    pthread_mutex_unlock(&g_engine.lock);
    return 0;
}

int engine_seek(long long ms)
{
    if (ms < 0)
        ms = 0;
    pthread_mutex_lock(&g_engine.lock);
    if (g_engine.state == ENGINE_IDLE) {
        pthread_mutex_unlock(&g_engine.lock);
        return -1;
    }
    g_engine.seek_ms = ms;
    g_engine.seek_req = 1;
    pthread_cond_broadcast(&g_engine.cond);
    pthread_mutex_unlock(&g_engine.lock);
    return 0;
}

int engine_stop(void)
{
    pthread_mutex_lock(&g_engine.lock);
    if (!g_engine.thread_running) {
        pthread_mutex_unlock(&g_engine.lock);
        return 0;
    }
    g_engine.stop_req = 1;
    g_engine.pause_flag = 0;
    pthread_cond_broadcast(&g_engine.cond);
    pthread_t t = g_engine.thread;
    pthread_mutex_unlock(&g_engine.lock);
    pthread_join(t, NULL);
    return 0;
}

int engine_set_volume(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    pthread_mutex_lock(&g_engine.lock);
    g_engine.volume = percent;
    pthread_mutex_unlock(&g_engine.lock);
    return 0;
}

int engine_get_state(void)
{
    int s;
    pthread_mutex_lock(&g_engine.lock);
    s = g_engine.state;
    pthread_mutex_unlock(&g_engine.lock);
    return s;
}

long long engine_position_ms(void)
{
    long long pos;
    unsigned int rate;
    pthread_mutex_lock(&g_engine.lock);
    pos = g_engine.position;
    rate = g_engine.rate;
    pthread_mutex_unlock(&g_engine.lock);
    return rate ? pos * 1000 / rate : 0;
}

long long engine_duration_ms(void)
{
    long long dur;
    unsigned int rate;
    pthread_mutex_lock(&g_engine.lock);
    dur = g_engine.duration;
    rate = g_engine.rate;
    pthread_mutex_unlock(&g_engine.lock);
    return (rate && dur) ? dur * 1000 / rate : 0;
}

const char* engine_last_error(void)
{
    return g_engine.last_error;
}
