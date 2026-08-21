#include "previewimageprovider.h"

#include "framestore.h"

/**
 * @brief 构造预览图像提供器。
 */
PreviewImageProvider::PreviewImageProvider(FrameStore *frameStore)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_frameStore(frameStore)
{
}

/**
 * @brief 响应 QML 的图像请求并返回最新摄像头画面。
 */
QImage PreviewImageProvider::requestImage(const QString &id, QSize *size,
                                          const QSize &requestedSize)
{
    Q_UNUSED(id)
    Q_UNUSED(requestedSize)

    const QImage image = m_frameStore != nullptr
            ? m_frameStore->latest().toImage() : QImage();
    if (size != nullptr)
        *size = image.size();
    return image;
}

