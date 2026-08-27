# camrecorder 独立摄像录像应用

## 功能说明

`camrecorder` 是面向正点原子 IMX6ULL Linux 开发板的独立 Qt 5.12 Quick 应用，首版不依赖综合项目的 `systemui`。

- 固定使用 `/dev/video1`。
- 固定采集 RGB565、640×480、30 FPS。
- 左侧预览区域固定为 640×480，画面不缩放、不铺满整个屏幕。
- Mode1 只采集和显示，不创建视频文件。
- Mode2 保持预览，并提供单独的开始、停止录像按钮。
- 录像格式为 640×480、15 FPS、JPEG 质量 85 的 MJPEG AVI。
- 视频只保存到已经挂载且可写的 `/mnt/usb`。
- 应用不会自动挂载 U 盘。

## 工程结构

```text
camrecorder/
├── camrecorder.pro
├── main.cpp
├── qml.qrc
├── qml/
│   └── main.qml
└── src/
    ├── cameracontroller.cpp
    ├── cameracontroller.h
    ├── camera/
    │   ├── cameracapture.cpp
    │   ├── cameracapture.h
    │   ├── framestore.cpp
    │   ├── framestore.h
    │   ├── previewimageprovider.cpp
    │   └── previewimageprovider.h
    └── recorder/
        ├── avirecorderworker.cpp
        ├── avirecorderworker.h
        ├── aviwriter.cpp
        └── aviwriter.h
```

## 虚拟机交叉编译

进入已安装正点原子 Qt SDK 的虚拟机，执行：

```bash
cd /home/alientek/my1demo/000final/004camrecorder/000crossBuild
source /opt/fsl-imx-x11/4.1.15-2.1.0/environment-setup-cortexa7hf-neon-poky-linux-gnueabi
qmake ../camrecorder/camrecorder.pro
make -j4
```

成功后当前目录会生成 `camrecorder` 可执行文件。

如果 SDK 或工程实际路径不同，只需调整 `cd` 和 `source` 后面的路径。

## libjpeg 路径

工程默认优先检查：

```text
/home/alientek/tools/jpeglib/include/jpeglib.h
/home/alientek/tools/jpeglib/lib/libjpeg.so
```

配置位置位于 `camrecorder.pro`：

```qmake
JPEG_ROOT = /home/alientek/tools/jpeglib
```

如果该目录不存在，工程会回退为 `-ljpeg`，使用交叉编译 SDK sysroot 中的 libjpeg。

如果虚拟机中的 libjpeg 安装在其他目录，请修改 `JPEG_ROOT`。如果链接阶段提示找不到 `jpeg_mem_dest`，说明 libjpeg 版本过旧，需要使用原 `camMulti` 已验证的 libjpeg 版本。

## U 盘准备

应用本身不会执行挂载。运行前请在开发板终端确认 U 盘已经挂载：

```bash
mkdir -p /mnt/usb
mount /dev/sda1 /mnt/usb
mountpoint /mnt/usb
touch /mnt/usb/camrecorder_write_test
rm /mnt/usb/camrecorder_write_test
```

设备分区不一定是 `/dev/sda1`，请根据开发板上的 `lsblk` 或 `/dev/sd*` 实际结果选择。

没有 U 盘时仍可使用 Mode1；Mode2 的开始录像按钮会被禁用。

## 开发板运行

将可执行文件复制到开发板后执行：

```bash
chmod +x camrecorder
./camrecorder
```

如果 Qt 平台插件没有由系统环境自动选择，需要按开发板桌面环境设置 `QT_QPA_PLATFORM`。综合项目正在 X11 下运行时通常不需要额外设置。

## 操作流程

1. 启动程序，默认处于 Mode1，摄像头尚未开启。
2. 点击“开启摄像头”，等待左侧显示 640×480 实时画面。
3. Mode1 下只预览，不会生成文件。
4. 点击 Mode2，摄像头不会重新启动，预览保持连续。
5. U 盘可用时点击“开始录像”。
6. 点击“停止录像”，等待界面从“保存中”恢复为 Mode2 空闲状态。
7. 完整录像保存在 `/mnt/usb/cam_rec_时间戳.avi`。

录像期间不能切换 Mode1、关闭摄像头或退出。必须先点击“停止录像”，等待 AVI 索引和文件头完成收尾。

## 临时文件说明

录像过程中使用：

```text
cam_rec_时间戳.avi.part
```

只有 AVI 完成收尾并同步到 U 盘后，程序才会将其改名为 `.avi`。如果发生编码、写盘或拔出 U 盘等错误，`.part` 文件不会被报告为成功录像，可保留用于排查，也可以在确认无用后手动删除。

## 板端验收建议

- Mode1 连续预览，确认没有生成 AVI 文件。
- 确认预览画面实际为 640×480，没有拉伸到全屏。
- Mode2 录制至少 10 秒，停止后用项目中的播放器检查 AVI。
- 拔掉 U 盘或不挂载 U 盘，确认 Mode1 仍可工作、Mode2 不能开始录像。
- 录像时尝试切换模式、关闭摄像头和退出，确认操作被禁止。
- 连续完成至少 5 次开启、预览、录像、停止、关闭流程，确认 `/dev/video1` 不会持续被占用。
