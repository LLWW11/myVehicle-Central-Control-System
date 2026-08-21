/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 1990-2030. All rights reserved.
* @projectName   SystemUI
* @brief         albumimage.cpp
* @author        Deng Zhimao
* @email         1252699831@qq.com/dengzhimao@alientek.com
* @date          2023-12-09
*******************************************************************/
#include "albumimage.h"
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QPixmap>
#include <QRectF>
#include <QRect>
AlbumImage::AlbumImage(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setFlag(ItemHasContents, true);
    connect(this, SIGNAL(radiusChanged()), this, SLOT(update()));
    connect(this, SIGNAL(sourceChanged()), this, SLOT(update()));
}

QVariant AlbumImage::source() const
{
    return m_source;
}

qreal AlbumImage::radius() const
{
    return m_radius;
}

void AlbumImage::setSource(const QVariant &source)
{
    m_source = source;
    emit sourceChanged();
}

void AlbumImage::setAlbum(qreal &radius)
{
    m_radius = radius;
    emit radiusChanged();
}

void AlbumImage::paint(QPainter *painter)
{
    if (painter == nullptr) {
        return;
    }
    if (m_source == "")
        return;

    if (!m_source.isValid())
        return;

    QString filePath;

    if (m_source.toUrl().isLocalFile())
        filePath = m_source.toUrl().toLocalFile();
    else
        filePath  = m_source.toString();

    QImage image(filePath);

    if (image.isNull())
        return;
    QPixmap src = QPixmap::fromImage(image);

    QPixmap pixmapa;
    pixmapa = src.scaled(width(), height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QPixmap pixmap(width(), height());
    pixmap.fill(Qt::transparent);
    painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    QPainterPath path;
    path.addRoundedRect(0, 0, width(), height(), m_radius, m_radius);
    //path.addEllipse(width() - m_radius, height() / 2 - m_radius, m_radius * 2, m_radius * 2);
    painter->setClipPath(path);
    painter->drawPixmap(0, 0, width(), height(), pixmapa);
}
