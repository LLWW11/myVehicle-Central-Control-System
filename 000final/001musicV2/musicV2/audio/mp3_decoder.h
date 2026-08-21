#ifndef MP3_DECODER_H
#define MP3_DECODER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* libmpg123 封装（纯 C）: MP3 文件 -> 交错 S16 PCM */

typedef struct Mp3Decoder Mp3Decoder;

/* 全局初始化/清理, 整个进程只调用一次 */
int  mp3_global_init(void);
void mp3_global_exit(void);

/* 打开文件并固定输出格式为 S16 交错; 输出采样率/声道数。
 * 失败返回 NULL, errbuf 非空时写入失败原因(可拼进错误提示) */
Mp3Decoder* mp3_open(const char* path, unsigned int* rate, int* channels,
                     char* errbuf, size_t errbuf_size);

/* 读 frames 个样本帧(每声道), 返回实际帧数;
 * 返回 0 = 文件结束(EOF), 返回 -1 = 解码错误
 * (原因见 mp3_last_error/mp3_last_code) */
long mp3_read(Mp3Decoder* d, void* pcm, unsigned long frames);

/* 跳转到毫秒位置, 返回实际到达的毫秒位置(可能略小于目标) */
long long mp3_seek_ms(Mp3Decoder* d, long long ms);

/* 总时长(毫秒)。首次调用会扫描整个文件(VBR 时较慢), 之后缓存。
 * 失败返回 0 */
long long mp3_duration_ms(Mp3Decoder* d);

/* 最近一次操作的错误描述/错误码(成功操作后为空/0) */
const char* mp3_last_error(const Mp3Decoder* d);
int         mp3_last_code(const Mp3Decoder* d);

void mp3_close(Mp3Decoder* d);

#ifdef __cplusplus
}
#endif

#endif /* MP3_DECODER_H */
