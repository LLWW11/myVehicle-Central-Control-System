#ifndef PREVIEWIMAGEPROVIDER_H
#define PREVIEWIMAGEPROVIDER_H

#include <QQuickImageProvider>

class FrameStore;
// 将 FrameStore 中的最新帧以 image://camera 地址提供给 QML。
class PreviewImageProvider : public QQuickImageProvider
{
public:
    /**
     * @brief 构造预览图像提供器。
     * @param frameStore 摄像头最新帧存储区，不转移所有权。
     */
    explicit PreviewImageProvider(FrameStore *frameStore);

    /**
     * @brief 响应 QML 的图像请求并返回最新摄像头画面。
     * @param id 图像资源标识，当前实现只用于破坏 QML 缓存。
     * @param size 返回原始图像尺寸。
     * @param requestedSize QML 请求尺寸；预览必须保持原尺寸，因此不缩放。
     * @return 最新的 640×480 图像或空图像。
     */
    QImage requestImage(const QString &id, QSize *size,
                        const QSize &requestedSize) override;

private:
    FrameStore *m_frameStore = nullptr;
};

#endif // PREVIEWIMAGEPROVIDER_H
