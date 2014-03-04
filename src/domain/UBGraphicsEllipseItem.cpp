/*
 * Copyright (C) 2010-2013 Groupement d'Intérêt Public pour l'Education Numérique en Afrique (GIP ENA)
 *
 * This file is part of Open-Sankoré.
 *
 * Open-Sankoré is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License,
 * with a specific linking exception for the OpenSSL project's
 * "OpenSSL" library (or with modified versions of it that use the
 * same license as the "OpenSSL" library).
 *
 * Open-Sankoré is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open-Sankoré.  If not, see <http://www.gnu.org/licenses/>.
 */



#include "UBGraphicsEllipseItem.h"

UB3HEditableGraphicsEllipseItem::UB3HEditableGraphicsEllipseItem(QGraphicsItem* parent):
    UB3HEditablesGraphicsBasicShapeItem(parent)
{
    // Ellipse has Stroke and Fill capabilities :
    initializeStrokeProperty();
    initializeFillingProperty();

    mRadiusX = 0;
    mRadiusY = 0;
}

UB3HEditableGraphicsEllipseItem::~UB3HEditableGraphicsEllipseItem()
{

}

UBItem *UB3HEditableGraphicsEllipseItem::deepCopy() const
{
    UB3HEditableGraphicsEllipseItem* copy = new UB3HEditableGraphicsEllipseItem();

    copyItemParameters(copy);

    return copy;
}

void UB3HEditableGraphicsEllipseItem::copyItemParameters(UBItem *copy) const
{
    UB3HEditablesGraphicsBasicShapeItem::copyItemParameters(copy);

    UB3HEditableGraphicsEllipseItem *cp = dynamic_cast<UB3HEditableGraphicsEllipseItem*>(copy);

    if(!cp) return;

    cp->mRadiusX = mRadiusX;
    cp->mRadiusY = mRadiusY;
}

QPointF UB3HEditableGraphicsEllipseItem::center() const
{
    QPointF centre;

    centre.setX(pos().x() + mRadiusX);
    centre.setY(pos().y() + mRadiusY);

    return centre;
}

void UB3HEditableGraphicsEllipseItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    setStyle(painter);

    painter->drawEllipse(QPointF(mRadiusX, mRadiusY), mRadiusX, mRadiusY);

    if(mMultiClickState %2 == 1){
        QPen p;
        p.setColor(QColor(128, 128, 200));
        p.setStyle(Qt::DotLine);
        p.setWidth(pen().width());

        painter->setPen(p);
        painter->setBrush(QBrush());

        painter->drawRect(0, 0, mRadiusX*2, mRadiusY*2);
    }
}

QRectF UB3HEditableGraphicsEllipseItem::boundingRect() const
{
    QRectF rect(0, 0, mRadiusX*2, mRadiusY*2);

    rect = adjustBoundingRect(rect);

    return rect;
}

void UB3HEditableGraphicsEllipseItem::onActivateEditionMode()
{
    verticalHandle()->setPos(mRadiusX, mRadiusY*2);
    horizontalHandle()->setPos(mRadiusX*2, mRadiusY);

    diagonalHandle()->setPos(mRadiusX*2, mRadiusY*2);
}

void UB3HEditableGraphicsEllipseItem::updateHandle(UBAbstractHandle *handle)
{
    prepareGeometryChange();

    qreal maxSize = handle->radius() * 4;

    if(handle->getId() == 1){
        //it's the vertical handle
        if(handle->pos().y() >= maxSize){
            mRadiusY = handle->pos().y() / 2;
        }
    }else if(handle->getId() == 0){
        //it's the horizontal handle
        if(handle->pos().x() >= maxSize){
            mRadiusX = handle->pos().x() / 2;
        }
    }else{
        //it's the diagonal handle
        if(handle->pos().x() >= maxSize && handle->pos().y() >= maxSize){
            mRadiusX = handle->pos().x() / 2;
            mRadiusY = handle->pos().y() / 2;
        }
    }

    verticalHandle()->setPos(mRadiusX, mRadiusY*2);
    horizontalHandle()->setPos(mRadiusX*2, mRadiusY);

    diagonalHandle()->setPos(mRadiusX*2, mRadiusY*2);

    if(hasGradient()){
        QLinearGradient g(QPointF(), QPointF(mRadiusX*2, 0));

        g.setColorAt(0, brush().gradient()->stops().at(0).second);
        g.setColorAt(1, brush().gradient()->stops().at(1).second);

        setBrush(g);
    }

    update();
}

QPainterPath UB3HEditableGraphicsEllipseItem::shape() const
{
    QPainterPath path;
    if(mMultiClickState %2 == 1){
        path.addRect(boundingRect());
    }else{
        path.addEllipse(boundingRect());
    }
    return path;
}

void UB3HEditableGraphicsEllipseItem::setRadiusX(qreal radius)
{
    prepareGeometryChange();
    mRadiusX = radius;
    update();
}

void UB3HEditableGraphicsEllipseItem::setRadiusY(qreal radius)
{
    prepareGeometryChange();
    mRadiusY = radius;
    update();
}

void UB3HEditableGraphicsEllipseItem::setRect(QRectF rect){
    setPos(rect.topLeft());
    mRadiusX = rect.width()/2;
    mRadiusY = rect.height()/2;

    if(hasGradient()){
        QLinearGradient g(QPointF(), QPointF(mRadiusX*2, 0));

        g.setColorAt(0, brush().gradient()->stops().at(0).second);
        g.setColorAt(1, brush().gradient()->stops().at(1).second);

        setBrush(g);
    }
}

QRectF UB3HEditableGraphicsEllipseItem::rect() const
{
    QRectF r;
    r.setTopLeft(pos());
    r.setWidth(mRadiusX*2);
    r.setHeight(mRadiusY*2);

    return r;
}

qreal UB3HEditableGraphicsEllipseItem::radiusX() const
{
    return mRadiusX;
}

qreal UB3HEditableGraphicsEllipseItem::radiusY() const
{
    return mRadiusY;
}
