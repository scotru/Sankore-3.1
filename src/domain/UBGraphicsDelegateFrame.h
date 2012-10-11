/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UBGRAPHICSDELEGATEFRAME_H_
#define UBGRAPHICSDELEGATEFRAME_H_

#include <QtGui>
#include "core/UB.h"
#include "domain/UBItem.h"

class QGraphicsSceneMouseEvent;
class UBGraphicsItemDelegate;
class QGraphicsSvgItem;
class DelegateButton;

class UBGraphicsDelegateFrame: public QGraphicsRectItem, public UBGraphicsItem, public QObject
{
    public:

        UBGraphicsDelegateFrame(UBGraphicsItemDelegate* pDelegate, QRectF pRect, qreal pFrameWidth, bool respectRatio = true);
        virtual ~UBGraphicsDelegateFrame();

        enum { Type = UBGraphicsItemType::DelegateFrameType };
        virtual int type() const {return Type;}

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                QWidget *widget);

        QPainterPath shape() const;

        virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);
        QPointF getFixedPointFromPos();
        QSizeF getResizeVector(qreal moveX, qreal moveY);
        QSizeF resizeDelegate(qreal moveX, qreal moveY);
        virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
        virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

        void positionHandles();
        void setVisible(bool visible);

                virtual void setAntiScale(qreal pAntiScale);

        enum OperationMode {Scaling, Resizing, ResizingHorizontally};
        void setOperationMode(OperationMode pMode) {mOperationMode = pMode;}
        bool isResizing(){return mResizing;}
        void moveLinkedItems(QLineF movingVector, bool bLinked = false);
        void prepareFramesToMove(QList<UBGraphicsDelegateFrame *> framesToMove);
        void prepareLinkedFrameToMove();
        QList<UBGraphicsDelegateFrame *> getLinkedFrames();

    private:
        QRectF bottomRightResizeGripRect() const;
        QRectF bottomResizeGripRect() const;
        QRectF leftResizeGripRect() const;
        QRectF rightResizeGripRect() const;
        QRectF topResizeGripRect() const;
        QRectF rotateButtonBounds() const;

        inline bool resizingBottomRight () const { return mCurrentTool == ResizeBottomRight; }
        inline bool resizingBottom ()  const { return mCurrentTool == ResizeBottom; }
        inline bool resizingRight () const { return mCurrentTool == ResizeRight; }
        inline bool resizingLeft () const { return mCurrentTool == ResizeLeft; }
        inline bool resizingTop () const { return mCurrentTool == ResizeTop; }
        inline bool rotating () const { return mCurrentTool == Rotate; }
        inline bool moving () const { return mCurrentTool == Move; }
        void setCursorFromAngle(QString angle);
        bool canResizeBottomRight(qreal width, qreal height, qreal scaleFactor);

        QTransform buildTransform ();
        void  updateResizeCursors ();
        void  initializeTransform ();

        enum FrameTool {None, Move, Rotate, ResizeBottomRight, ResizeTop, ResizeRight, ResizeBottom, ResizeLeft};
        FrameTool toolFromPos (QPointF pos);
        void refreshGeometry();

        FrameTool mCurrentTool;
        UBGraphicsItemDelegate* mDelegate;

        bool mVisible;
        qreal mFrameWidth;
        qreal mNominalFrameWidth;
        bool mRespectRatio;

        qreal mAngle;
        qreal mAngleOffset;
        qreal mTotalScaleX;
        qreal mTotalScaleY;
        qreal mScaleX;
        qreal mScaleY;
        qreal mTranslateX;
        qreal mTranslateY;
        qreal mTotalTranslateX;
        qreal mTotalTranslateY;
        qreal mAngleTolerance;
        QRect mAngleRect;

        QPointF mStartingPoint;
        QTransform mInitialTransform;
        QSizeF mOriginalSize;
        QPointF mFixedPoint;

        DelegateButton* mBottomRightResizeGripSvgItem;
        DelegateButton* mBottomResizeGripSvgItem;
        DelegateButton* mLeftResizeGripSvgItem;
        DelegateButton* mRightResizeGripSvgItem;
        DelegateButton* mTopResizeGripSvgItem;
        DelegateButton* mRotateButton;

        QGraphicsRectItem* mBottomRightResizeGrip;
        QGraphicsRectItem* mBottomResizeGrip;
        QGraphicsRectItem* mLeftResizeGrip;
        QGraphicsRectItem* mRightResizeGrip;
        QGraphicsRectItem* mTopResizeGrip;

        OperationMode mOperationMode;

        QGraphicsItem* delegated();
        bool mFlippedX;
        bool mFlippedY;
        bool mMirrorX;
        bool mMirrorY;
        bool mResizing;
        bool mMirroredXAtStart;
        bool mMirroredYAtStart;

        QList<UBGraphicsDelegateFrame *> mLinkedFrames;
};
#endif /* UBGRAPHICSDELEGATEFRAME_H_ */
