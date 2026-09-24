# musicV3 独立播放器

musicV3 是 Qt Quick 前台音乐播放器。网络歌曲先下载为本地 MP3，再由 libmpg123 解码为交错 S16 PCM，最后通过 ALSA 播放。窗口隐藏或最小化时停止音频并取消当前网络请求；重新显示后需要用户再次点击播放。

## 构建

在 Linux 开发板或匹配开发板的交叉编译环境中，准备 Qt 5 的 Core、Gui、Quick、Network 模块，以及 ALSA 和 libmpg123 的头文件、库文件。

```sh
mkdir -p ../000crossBuild
cd ../000crossBuild
qmake ../musicV3/musicV3.pro MPGLIB_DIR=/home/alientek/tools/mpglib
make -j4
```

`MPGLIB_DIR` 可按实际安装位置覆盖。项目目标名为 `musicV3`。源码使用 C++11；ALSA 和 libmpg123 的外部接口由 C++ 类封装，项目不再编译旧的 C 播放引擎。

## 运行配置

播放预置网络歌曲需要音源密钥，解析优先级为：环境变量 `CERU_MUSIC_API_KEY` > 应用目录下的纯文本文件 `music_api_key.txt`（取首行，部署后即 `/opt/ui/src/apps/music_api_key.txt`）> 源码内置的默认密钥。默认配置可直接播放；需要轮换密钥或撤下内置密钥时，在板上放置该文件即可，无需重新编译。SystemUI 通过 `QProcess` 启动子应用并继承其环境变量，因此 `export CERU_MUSIC_API_KEY=...` 必须写在启动 SystemUI 之前（如启动脚本或 `/etc/profile`），运行中再导出无效。播放本地 MP3 不需要该密钥。

MP3、歌词和封面缓存在 `QStandardPaths::CacheLocation` 下的 `musicV3` 子目录（板上通常为 `~/.cache/Ceru/musicV3/musicV3`）。若运行环境没有 `HOME`/`XDG_CACHE_HOME`，缓存目录回退到应用目录的 `cache/musicV3`。

默认 ALSA 设备名为 `default`。可按开发板声卡配置设置 `CERU_ALSA_DEV`，例如 `hw:0,0`。直接选择硬件设备时，设备必须支持歌曲实际的采样率、声道数和 S16_LE 格式。

封面会转换为 JPEG 缓存；若开发板缺少 Qt JPEG 图像插件，歌曲播放仍可继续，但封面可能显示默认图片。

## 测试

`tests/audio_tests.pro` 是独立的 Qt Test 工程，检查 S16 帧大小、缓存身份和列表索引循环。真实 ALSA 写入、欠载恢复、暂停、切歌和退出延迟需要在开发板上验证。
