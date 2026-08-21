# 精简地图 V1

本目录是从 Qt Location Map Viewer 教程裁剪得到的最小地图版本。V1 面向 `1024×600` 屏幕，左侧使用固定 `640×480` 地图区域，右侧显示地图信息与控制按钮。

## V1 已保留功能

- 固定使用 `osm` Provider。
- 固定瓦片地址为 `https://tile.openstreetmap.de/`。
- 自动选择名称包含 `custom` 的地图类型，不再需要手动进入 `MapType` 菜单。
- 支持地图平移和双指缩放。
- 支持右侧加减按钮进行整数级缩放，并提供初始视图复位按钮。
- 右侧显示地图中心经纬度、缩放等级、地图类型、地图状态和定位状态。
- 地图中心坐标在停止操作约 200 ms 后更新，避免拖动期间逐帧重排文本。
- 保留地图版权信息显示。
- 地图或 Provider 异常时显示简短错误信息。

## V1 已停用功能

- Provider、MapType、Tools 菜单。
- 路线规划、地理编码和反向地理编码。
- 小地图、视角滑块、旋转、倾斜和惯性滑动。
- 标记点菜单及矩形、圆、折线、多边形等绘图功能。
- Qt Positioning 自动定位和串口定位。

旧教程的菜单、表单、路线、绘图、小地图、图片资源、文档、IDE 用户配置和复制来的 Debug 构建产物均已从 Lite 目录移除。原始实现仍保留在同级的 `003mapTest` 项目中，需要时可以对照或取回。

## 虚拟机编译

建议新建干净的 Release 构建目录，不要复用复制过来的桌面 Debug 构建目录：

```bash
cd ~/my1demo/000final/003mapTestLite
mkdir -p 000crossBuild
cd 000crossBuild
qmake ../mapviewer/mapviewer.pro CONFIG+=release CONFIG-=debug
make -j2
```

生成的程序名称为：

```text
maplite
```

Provider 参数已经写入 QML，运行时不再需要追加 `--plugin.*` 参数：

```bash
./maplite
```

## 可调整参数

初始经纬度和缩放等级位于 `mapviewer.qml` 顶部：

```qml
readonly property double initialLatitude: 26.0745
readonly property double initialLongitude: 119.2965
```

地图默认缩放等级为 `15`。如果开发板双指缩放仍然卡顿，可以把 `gesture.acceptedGestures` 改为只保留 `MapGestureArea.PanGesture`，完全使用右侧按钮缩放。

右侧当前显示的是地图中心坐标，不是 GPS 定位坐标。接入 V2串口定位后，将新增独立定位坐标与定位状态，避免把用户手动拖动后的地图中心误认为设备位置。

## 下一版本方向

V2 将增加独立的串口定位类，解析模块输出的经纬度，只在地图上维护一个当前位置标记，并提供可控的位置跟随功能。
