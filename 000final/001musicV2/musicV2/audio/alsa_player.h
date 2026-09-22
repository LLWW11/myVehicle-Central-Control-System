#ifndef ALSA_PLAYER_H
#define ALSA_PLAYER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 不透明 ALSA 设备句柄 */
typedef struct AlsaDevice AlsaDevice;

/* dev: "default"(默认) / "hw:0" / 环境变量 CERU_ALSA_DEV 覆盖;
 * rate/channels 由 MP3 解码出的格式决定。失败返回 NULL */
AlsaDevice* alsa_open(const char* dev, unsigned int rate, int channels);

/* 写 frames 个交错样本帧, 返回实际写帧数(通常==frames), <0 表示错误 */
long alsa_write(AlsaDevice* d, const void* data, unsigned long frames);

/* 硬件暂停/恢复, 返回 1 表示已支持并执行, 0 表示设备不支持
 * (引擎默认用软件暂停, 这两个函数供可选优化) */
int  alsa_pause(AlsaDevice* d);
int  alsa_resume(AlsaDevice* d);

/* 丢弃缓冲中未播数据并重新 prepare, seek 后调用避免播放旧数据 */
void alsa_drop_and_prepare(AlsaDevice* d);

void alsa_close(AlsaDevice* d);

#ifdef __cplusplus
}
#endif

#endif /* ALSA_PLAYER_H */
