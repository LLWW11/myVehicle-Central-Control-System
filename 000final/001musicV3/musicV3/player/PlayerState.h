#ifndef PLAYERSTATE_H
#define PLAYERSTATE_H

#include <QMetaType>

enum class PlayerState
{
    Stopped,  // 没有活动播放任务
    Starting, // 已接收到播放请求，正在打开文件或声卡
    Playing,  // 正在解码并向 ALSA 写入 PCM 数据
    Paused,   // 解码线程正在等待恢复、跳转或停止命令
    Stopping  // 正在释放解码器和声卡
};
enum class PlaybackFinishReason : qint8
{
    EndOfStream,   // 放完了
    UserStopped,   // 手动停止
    SourceChanged, // 切歌
    WindowHidden,  // 窗口隐藏
    ShutDown,      // 退出
    Error          // 出错
};
enum class PlaybackError : qint8
{
    None,              // 没错误
    FileOpenFailed,    // 音频文件打开错误
    DecodeFailed,      // MP3解码错误
    Unsupported,       // 解码格式错误
    AudioDeviceFailed, // ALSA设备打开错误
    PCMWriteFailed,    // PCM写入错误
};
// 跨线程信号参数
Q_DECLARE_METATYPE(PlayerState)
Q_DECLARE_METATYPE(PlaybackFinishReason)
Q_DECLARE_METATYPE(PlaybackError)
#endif // PLAYERSTATE_H
