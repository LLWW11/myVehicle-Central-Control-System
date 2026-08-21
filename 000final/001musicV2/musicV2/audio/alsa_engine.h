#ifndef ALSA_ENGINE_H
#define ALSA_ENGINE_H

/*
 * alsa_engine.h - 纯 C 播放引擎公共接口（不依赖 Qt）
 *
 * 线程模型:
 *   engine_play 内部创建一条播放线程, 线程内完成 MP3 解码 + ALSA 写出;
 *   暂停/继续/跳转/停止 通过互斥锁 + 条件变量与播放线程通信;
 *   所有 snd_pcm 调用只发生在播放线程, 外部线程可安全调用下列接口。
 *
 * 注意: 三个回调(状态/错误/结束)运行在播放线程上,
 *       回调内禁止调用本引擎的任何接口, 只能做通知/信号这类非阻塞动作。
 */

#ifdef __cplusplus
extern "C" {
#endif

enum {
    ENGINE_IDLE    = 0,   /* 未播放/已停止 */
    ENGINE_PLAYING = 1,
    ENGINE_PAUSED  = 2,
    ENGINE_ERROR   = 3    /* 出错后短暂过渡状态, 随即回到 IDLE */
};

typedef void (*engine_state_cb)(int state, void* user);
typedef void (*engine_error_cb)(const char* msg, void* user);
typedef void (*engine_end_cb)(void* user);

/* 初始化引擎(注册回调)。返回 0 成功; 内部做 mpg123 全局初始化 */
int  engine_init(engine_state_cb scb, engine_error_cb ecb,
                 engine_end_cb endcb, void* user);
/* 反初始化: 停止并回收播放线程, 做 mpg123 全局清理 */
void engine_deinit(void);

/* 开始(或重播)一个 MP3 文件, 立即返回。内部会先停止上一首 */
int  engine_play(const char* path);
/* 暂停/继续, 0 成功, -1 状态不允许 */
int  engine_pause(void);
int  engine_resume(void);
/* 跳转毫秒位置(文件内), 0 成功 */
int  engine_seek(long long ms);
/* 停止播放并等待播放线程退出 */
int  engine_stop(void);
/* 音量 0-100, 软件缩放 PCM 样本 */
int  engine_set_volume(int percent);

int       engine_get_state(void);
long long engine_position_ms(void);
long long engine_duration_ms(void);
/* 最近一次错误描述 */
const char* engine_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* ALSA_ENGINE_H */
