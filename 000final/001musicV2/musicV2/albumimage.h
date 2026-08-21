/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 1990-2030. All rights reserved.
* @projectName   SystemUI
* @brief         albumimage.h
* @author        Deng Zhimao
* @email         1252699831@qq.com/dengzhimao@alientek.com
* @date          2023-12-09
*******************************************************************/
#ifndef ALBUMIMAGE_H
#define ALBUMIMAGE_H

#include <QQuickPaintedItem>
#include <QImage>
#include <QQuickItem>
#include <QVariant>

class AlbumImage : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QVariant source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(qreal radius READ radius WRITE setAlbum NOTIFY radiusChanged)
public:
    explicit AlbumImage(QQuickItem *parent = nullptr);
    QVariant source() const;
    qreal radius() const;

    void setSource(const QVariant &source);
    void setAlbum(qreal &radius);
protected:
    void paint(QPainter *painter) override;
signals:
    void sourceChanged();
    void radiusChanged();

private:
    QVariant m_source;
    qreal m_radius;
};

#endif // ALBUMIMAGE_H
