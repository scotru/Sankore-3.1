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



#include "UBGraphicsScene.h"

#include <QtGui>
#include <QtWebKit>
#include <QtSvg>
#include <QGraphicsView>

#include "frameworks/UBGeometryUtils.h"
#include "frameworks/UBPlatformUtils.h"

#include "core/UBApplication.h"
#include "core/UBSettings.h"
#include "core/UBApplicationController.h"
#include "core/UBDisplayManager.h"
#include "core/UBPersistenceManager.h"
#include "core/UBTextTools.h"

#include "gui/UBMagnifer.h"
#include "gui/UBMainWindow.h"
#include "gui/UBToolWidget.h"

#include "tools/UBGraphicsRuler.h"
#include "tools/UBGraphicsProtractor.h"
#include "tools/UBGraphicsCompass.h"
#include "tools/UBGraphicsTriangle.h"
#include "tools/UBGraphicsCurtainItem.h"
#include "tools/UBGraphicsCache.h"
#include "tools/UBGraphicsAristo.h"

#include "document/UBDocumentProxy.h"

#include "board/UBBoardController.h"
#include "board/UBDrawingController.h"
#include "board/UBBoardView.h"

#include "UBGraphicsItemUndoCommand.h"
#include "UBGraphicsItemGroupUndoCommand.h"
#include "UBGraphicsTextItemUndoCommand.h"
#include "UBGraphicsProxyWidget.h"
#include "UBGraphicsPixmapItem.h"
#include "UBGraphicsSvgItem.h"
#include "UBGraphicsPolygonItem.h"
#include "UBGraphicsMediaItem.h"
#include "UBGraphicsWidgetItem.h"
#include "UBGraphicsPDFItem.h"
#include "UBGraphicsTextItem.h"
#include "UBGraphicsStrokesGroup.h"

#include "customWidgets/UBGraphicsItemAction.h"

#include "domain/UBGraphicsGroupContainerItem.h"

#include "domain/UBAbstractGraphicsPathItem.h"
#include "domain/UBGraphicsEllipseItem.h"
#include "domain/UBGraphicsRectItem.h"
#include "domain/UBGraphicsLineItem.h"

#include "UBGraphicsStroke.h"

#include "core/memcheck.h"

qreal UBZLayerController::errorNumber = -20000001.0;

UBZLayerController::UBZLayerController(QGraphicsScene *scene) :
    mScene(scene)

{
    scopeMap.insert(itemLayerType::NoLayer,        ItemLayerTypeData( errorNumber, errorNumber));
    scopeMap.insert(itemLayerType::BackgroundItem, ItemLayerTypeData(-10000000.0, -10000000.0 ));
    scopeMap.insert(itemLayerType::ObjectItem,     ItemLayerTypeData(-10000000.0,  0.0        ));
    scopeMap.insert(itemLayerType::DrawingItem,    ItemLayerTypeData( 0.0,         10000000.0 ));
    scopeMap.insert(itemLayerType::ToolItem,       ItemLayerTypeData( 10000000.0,  10000100.0 ));
    scopeMap.insert(itemLayerType::CppTool,        ItemLayerTypeData( 10000100.0,  10000200.0 ));
    scopeMap.insert(itemLayerType::Curtain,        ItemLayerTypeData( 10000200.0,  10001000.0 ));
    scopeMap.insert(itemLayerType::Eraiser,        ItemLayerTypeData( 10001000.0,  10001100.0 ));
    scopeMap.insert(itemLayerType::Pointer,        ItemLayerTypeData( 10001100.0,  10001200.0 ));
    scopeMap.insert(itemLayerType::Cache,          ItemLayerTypeData( 10001300.0,  10001400.0 ));

    scopeMap.insert(itemLayerType::SelectedItem,   ItemLayerTypeData( 10001000.0,  10001000.0 ));
}

qreal UBZLayerController::generateZLevel(itemLayerType::Enum key)
{

    if (!scopeMap.contains(key)) {
        qDebug() << "Number is out of layer scope";
        return errorNumber;
    }

    qreal result = scopeMap.value(key).curValue;
    qreal top = scopeMap.value(key).topLimit;
    qreal incrementalStep = scopeMap.value(key).incStep;

    result += incrementalStep;
    if (result >= top) {
        // If not only one variable presents in the scope, notify that values for scope are over
        if (scopeMap.value(key).topLimit != scopeMap.value(key).bottomLimit) {
            qDebug() << "new values are over for the scope" << key;
        }
        result = top - incrementalStep;
    }

    scopeMap[key].curValue = result;

    return result;
}
qreal UBZLayerController::generateZLevel(QGraphicsItem *item)
{
    qreal result = errorNumber;
    itemLayerType::Enum type = static_cast<itemLayerType::Enum>(item->data(UBGraphicsItemData::itemLayerType).toInt());

    if (validLayerType(type)) {
        result =  generateZLevel(type);
    }

    return result;
}

qreal UBZLayerController::changeZLevelTo(QGraphicsItem *item, moveDestination dest)
{
    itemLayerType::Enum curItemLayerType = typeForData(item);
    if (curItemLayerType == itemLayerType::NoLayer) {
        qDebug() << "item's layer is out of the scope. Can't implement z-layer changing operation";
        return errorNum();
    }

    //select only items wiht the same z-level as item's one and push it to sortedItems QMultiMap
    QMultiMap<qreal, QGraphicsItem*> sortedItems;
    if (mScene->items().count()) {
        foreach (QGraphicsItem *tmpItem, mScene->items()) {
            if (typeForData(tmpItem) == curItemLayerType) {

                sortedItems.insert(tmpItem->data(UBGraphicsItemData::ItemOwnZValue).toReal(), tmpItem);
            }
        }
    }

    //If only one item itself - do nothing, return it's z-value
    if (sortedItems.count() == 1 && sortedItems.values().first() == item) {
        qDebug() << "only one item exists in layer. Have nothing to change";
        return item->data(UBGraphicsItemData::ItemOwnZValue).toReal();
    }

    QMapIterator<qreal, QGraphicsItem*>iCurElement(sortedItems);

    if (dest == up) {
        if (iCurElement.findNext(item)) {
            if (iCurElement.hasNext()) {
                qreal nextZ = iCurElement.peekNext().value()->data(UBGraphicsItemData::ItemOwnZValue).toReal();
                UBGraphicsItem::assignZValue(iCurElement.peekNext().value(), item->data(UBGraphicsItemData::ItemOwnZValue).toReal());
                UBGraphicsItem::assignZValue(item, nextZ);

                iCurElement.next();

                while (iCurElement.hasNext() && iCurElement.peekNext().value()->data(UBGraphicsItemData::ItemOwnZValue).toReal() == nextZ) {
                    UBGraphicsItem::assignZValue(iCurElement.next().value(), nextZ);
                }
            }
        }

    } else if (dest == top) {
        if (iCurElement.findNext(item)) {
            if (iCurElement.hasNext()) {
                UBGraphicsItem::assignZValue(item, generateZLevel(item));
            }
        }

    } else if (dest == down) {
        iCurElement.toBack();
        if (iCurElement.findPrevious(item)) {
            if (iCurElement.hasPrevious()) {
                qreal nextZ = iCurElement.peekPrevious().value()->data(UBGraphicsItemData::ItemOwnZValue).toReal();
                UBGraphicsItem::assignZValue(iCurElement.peekPrevious().value(), item->data(UBGraphicsItemData::ItemOwnZValue).toReal());
                UBGraphicsItem::assignZValue(item, nextZ);

                while (iCurElement.hasNext() && iCurElement.peekNext().value()->data(UBGraphicsItemData::ItemOwnZValue).toReal() == nextZ) {
                        UBGraphicsItem::assignZValue(iCurElement.next().value(), nextZ);
                }
            }
        }

    } else if (dest == bottom) {
        iCurElement.toBack();
        if (iCurElement.findPrevious(item)) {
            if (iCurElement.hasPrevious()) {
                qreal oldz = item->data(UBGraphicsItemData::ItemOwnZValue).toReal();
                iCurElement.toFront();
                qreal nextZ = iCurElement.next().value()->data(UBGraphicsItemData::ItemOwnZValue).toReal();

                ItemLayerTypeData curItemLayerTypeData = scopeMap.value(curItemLayerType);

                //if we have some free space between lowest graphics item and layer's bottom bound,
                //insert element close to first element in layer
                if (nextZ > curItemLayerTypeData.bottomLimit + curItemLayerTypeData.incStep) {
                    qreal result = nextZ - curItemLayerTypeData.incStep;
                    UBGraphicsItem::assignZValue(item, result);
                } else {
                    UBGraphicsItem::assignZValue(item, nextZ);

                    bool doubleGap = false; //to detect if we can finish rundown since we can insert item to the free space

                    while (iCurElement.peekNext().value() != item) {
                        qreal curZ = iCurElement.value()->data(UBGraphicsItemData::ItemOwnZValue).toReal();
                        qreal curNextZ = iCurElement.peekNext().value()->data(UBGraphicsItemData::ItemOwnZValue).toReal();
                        if (curNextZ - curZ >= 2 * curItemLayerTypeData.incStep) {
                            UBGraphicsItem::assignZValue(iCurElement.value(), curZ + curItemLayerTypeData.incStep);
                            doubleGap = true;
                            break;
                        } else {
                            UBGraphicsItem::assignZValue(iCurElement.value(), curNextZ);
                            iCurElement.next();
                        }
                    }
                    if (!doubleGap) {

                        UBGraphicsItem::assignZValue(iCurElement.value(), oldz);

                        while (iCurElement.hasNext() && (iCurElement.peekNext().value()->data(UBGraphicsItemData::ItemOwnZValue).toReal() == oldz)) {
                            UBGraphicsItem::assignZValue(iCurElement.next().value(), oldz);
                        }
                    }
                }
            }
        }
    }


    //clear selection of the item and then select it again to activate selectionChangeProcessing()
    item->scene()->clearSelection();
    item->setSelected(true);

    //Return new z value assigned to item
    return item->data(UBGraphicsItemData::ItemOwnZValue).toReal();

}

itemLayerType::Enum UBZLayerController::typeForData(QGraphicsItem *item) const
{
    itemLayerType::Enum result = static_cast<itemLayerType::Enum>(item->data(UBGraphicsItemData::itemLayerType).toInt());

    if (!scopeMap.contains(result)) {
        result = itemLayerType::NoLayer;
    }

    return result;
}

void UBZLayerController::setLayerType(QGraphicsItem *pItem, itemLayerType::Enum pNewType)
{
   pItem->setData(UBGraphicsItemData::itemLayerType, QVariant(pNewType));
}

UBGraphicsScene::UBGraphicsScene(UBDocumentProxy* parent, bool enableUndoRedoStack)
    : UBCoreGraphicsScene(parent)
    , mEraser(0)
    , mPointer(0)
    , mDocument(parent)
    , mDarkBackground(false)
    , mCrossedBackground(false)
    , mIsDesktopMode(false)
    , mZoomFactor(1)
    , mBackgroundObject(0)
    , mBackgroundObjectDisposition(Center)
    , mBackgroundObjectUrl(QUrl())
    , mPreviousWidth(0)
    , mInputDeviceIsPressed(false)
    , mArcPolygonItem(0)
    , mRenderingContext(Screen)
    , mCurrentStroke(0)
    , mShouldUseOMP(true)
    , mUndoRedoStackEnabled(enableUndoRedoStack)
    , magniferControlViewWidget(0)
    , magniferDisplayViewWidget(0)
    , mZLayerController(new UBZLayerController(this))
    , mpLastPolygon(NULL)
{
    UBCoreGraphicsScene::setObjectName("BoardScene");
#ifdef __ppc__
    mShouldUseOMP = false;
#elif defined(Q_WS_MAC)
    mShouldUseOMP = QSysInfo::MacintoshVersion >= QSysInfo::MV_10_5;
#endif

    setUuid(QUuid::createUuid());
    setDocument(parent);
    createEraiser();
    createPointer();

    if (UBApplication::applicationController)
    {
        setViewState(SceneViewState(1,
            UBApplication::applicationController->initialHScroll(),
            UBApplication::applicationController->initialVScroll(),
                                    QPointF()));//Issue 1598/1605 - CFA - 20131028
    }

//    Just for debug. Do not delete please
//    connect(this, SIGNAL(selectionChanged()), this, SLOT(selectionChangedProcessing()));
    connect(this, SIGNAL(selectionChanged()), this, SLOT(updateGroupButtonState()));
}

UBGraphicsScene::~UBGraphicsScene()
{
    if (mCurrentStroke && mCurrentStroke->polygons().empty()){
        delete mCurrentStroke;
        mCurrentStroke = NULL;
    }

    if (mZLayerController)
        delete mZLayerController;
        mZLayerController = NULL;
}

void UBGraphicsScene::selectionChangedProcessing()
{
    if (selectedItems().count()){
        UBApplication::showMessage("ZValue is " + QString::number(selectedItems().first()->zValue(), 'f') + "own z value is "
                                   + QString::number(selectedItems().first()->data(UBGraphicsItemData::ItemOwnZValue).toReal(), 'f'));

    }
}

// Issue 1598/1605 - CFA - 20131028
void UBGraphicsScene::setLastCenter(QPointF center)
{
    mViewState.setLastSceneCenter(center);
}

QPointF UBGraphicsScene::lastCenter()
{
    return mViewState.lastSceneCenter();
}
// Fin issue 1598/1605 - CFA - 20131028

void UBGraphicsScene::updateGroupButtonState()
{

    UBStylusTool::Enum currentTool = (UBStylusTool::Enum)UBDrawingController::drawingController()->stylusTool();
    if (UBStylusTool::Selector != currentTool && UBStylusTool::Play != currentTool)
        return;

    QAction *groupAction = UBApplication::mainWindow->actionGroupItems;
    QList<QGraphicsItem*> selItems = selectedItems();
    int selCount = selItems.count();

    if (selCount < 1) {
        //issue NC - NNE - 20131128 : correction bug of group action icon
        groupAction->setChecked(false);
        groupAction->setEnabled(false);
        groupAction->setText(UBApplication::app()->boardController->actionGroupText());

    } else if (selCount == 1) {
        if (selItems.first()->type() == UBGraphicsGroupContainerItem::Type) {
            //issue NC - NNE - 20131128 : correction bug of group action icon
            groupAction->setChecked(true);
            groupAction->setEnabled(true);
            groupAction->setText(UBApplication::app()->boardController->actionUngroupText());
        } else {
            groupAction->setEnabled(false);
        }

    } else if (selCount > 1) {
        groupAction->setEnabled(true);
        groupAction->setText(UBApplication::app()->boardController->actionGroupText());
    }
}

bool UBGraphicsScene::inputDevicePress(const QPointF& scenePos, const qreal& pressure)
{
    //mMesure1Ms = 0;
    //mMesure2Ms = 0;

    bool accepted = false;

    if (mInputDeviceIsPressed)
    {
        qWarning() << "scene received input device pressed, without input device release, muting event as input device move";
        accepted = inputDeviceMove(scenePos, pressure);
    }
    else
    {
        mInputDeviceIsPressed = true;

        UBStylusTool::Enum currentTool = (UBStylusTool::Enum)UBDrawingController::drawingController()->stylusTool();

        if (UBDrawingController::drawingController()->isDrawingTool())
        {
            // -----------------------------------------------------------------
            // We fall here if we are using the Pen, the Marker or the Line tool
            // -----------------------------------------------------------------
            qreal width = 0;

            // delete current stroke, if not assigned to any polygon
            if (mCurrentStroke && mCurrentStroke->polygons().empty()){
                delete mCurrentStroke;
                mCurrentStroke = NULL;
            }

            // ---------------------------------------------------------------
            // Create a new Stroke. A Stroke is a collection of QGraphicsLines
            // ---------------------------------------------------------------
            mCurrentStroke = new UBGraphicsStroke();

            if (currentTool != UBStylusTool::Line){
                // Handle the pressure
                width = UBDrawingController::drawingController()->currentToolWidth() * pressure;
            }else{
                // Ignore pressure for the line tool
                width = UBDrawingController::drawingController()->currentToolWidth();
            }

            width /= UBApplication::boardController->systemScaleFactor();
            width /= UBApplication::boardController->currentZoom();

            mAddedItems.clear();
            mRemovedItems.clear();

            if (UBDrawingController::drawingController()->mActiveRuler)
            {
                UBDrawingController::drawingController()->mActiveRuler->StartLine(scenePos, width);
            }
            else
            {
                moveTo(scenePos);
                drawLineTo(scenePos, width, UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Line);
            }
            accepted = true;
        }
        else if (currentTool == UBStylusTool::Eraser)
        {
            mAddedItems.clear();
            mRemovedItems.clear();
            moveTo(scenePos);

            qreal eraserWidth = UBSettings::settings()->currentEraserWidth();
            eraserWidth /= UBApplication::boardController->systemScaleFactor();
            eraserWidth /= UBApplication::boardController->currentZoom();

            eraseLineTo(scenePos, eraserWidth);
            drawEraser(scenePos, true);

            accepted = true;
        }
        else if (currentTool == UBStylusTool::Pointer)
        {
            drawPointer(scenePos, true);
            accepted = true;
        }
    }

    if (mCurrentStroke && mCurrentStroke->polygons().empty()){
        delete mCurrentStroke;
        mCurrentStroke = NULL;
    }

    return accepted;
}

bool UBGraphicsScene::inputDeviceMove(const QPointF& scenePos, const qreal& pressure)
{
    bool accepted = false;

    UBDrawingController *dc = UBDrawingController::drawingController();
    UBStylusTool::Enum currentTool = (UBStylusTool::Enum)dc->stylusTool();

    QPointF position = QPointF(scenePos);

    if (currentTool == UBStylusTool::Eraser)
    {
        drawEraser(position);
        accepted = true;
    }

    if (mInputDeviceIsPressed)
    {
        if (dc->isDrawingTool())
        {
            qreal width = 0;

            if (currentTool != UBStylusTool::Line){
                // Handle the pressure
                width = dc->currentToolWidth() * pressure;
            }else{
                // Ignore pressure for line tool
                width = dc->currentToolWidth();
            }

            width /= UBApplication::boardController->systemScaleFactor();
            width /= UBApplication::boardController->currentZoom();

            if (currentTool == UBStylusTool::Line || dc->mActiveRuler)
            {
                if (UBDrawingController::drawingController()->stylusTool() != UBStylusTool::Marker)
                if(NULL != mpLastPolygon && NULL != mCurrentStroke && mAddedItems.size() > 0){
                    UBCoreGraphicsScene::removeItemFromDeletion(mpLastPolygon);
                    mAddedItems.remove(mpLastPolygon);
                    mCurrentStroke->remove(mpLastPolygon);
                    if (mCurrentStroke->polygons().empty()){
                        delete mCurrentStroke;
                        mCurrentStroke = NULL;
                    }
                    removeItem(mpLastPolygon);
                    mPreviousPolygonItems.removeAll(mpLastPolygon);
                }

                // ------------------------------------------------------------------------
                // Here we wanna make sure that the Line will 'grip' at i*45, i*90 degrees
                // ------------------------------------------------------------------------

                QLineF radius(mPreviousPoint, position);
                qreal angle = radius.angle();
                angle = qRound(angle / 45) * 45;
                qreal radiusLength = radius.length();
                QPointF newPosition(
                    mPreviousPoint.x() + radiusLength * cos((angle * PI) / 180),
                    mPreviousPoint.y() - radiusLength * sin((angle * PI) / 180));
                QLineF chord(position, newPosition);
                                    if (chord.length() < qMin((int)16, (int)(radiusLength / 20)))
                    position = newPosition;
            }

            if(dc->mActiveRuler){
                dc->mActiveRuler->DrawLine(position, width);
            }else{
                drawLineTo(position, width, UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Line);
            }
        }
        else if (currentTool == UBStylusTool::Eraser)
        {
            qreal eraserWidth = UBSettings::settings()->currentEraserWidth();
            eraserWidth /= UBApplication::boardController->systemScaleFactor();
            eraserWidth /= UBApplication::boardController->currentZoom();

            eraseLineTo(position, eraserWidth);
        }
        else if (currentTool == UBStylusTool::Pointer)
        {
            drawPointer(position);
        }

        accepted = true;
    }

    return accepted;
}

bool UBGraphicsScene::inputDeviceRelease()
{
    /*
    if (mMesure1Ms > 0 ||  mMesure2Ms > 0)
    {
        qWarning() << "---------------------------";
        qWarning() << "mMesure1Ms: " << mMesure1Ms;
        qWarning() << "mMesure2Ms: " << mMesure2Ms;

        mMesure1Ms = 0;
        mMesure2Ms = 0;
    }
    */

    bool accepted = false;

    if (mPointer)
    {
        mPointer->hide();
        accepted = true;
    }

    UBDrawingController *dc = UBDrawingController::drawingController();

    if (dc->isDrawingTool() || mDrawWithCompass)
    {
        if(mArcPolygonItem){

                UBGraphicsStrokesGroup* pStrokes = new UBGraphicsStrokesGroup();

                // Add the arc
                mAddedItems.remove(mArcPolygonItem);
                removeItem(mArcPolygonItem);
                UBCoreGraphicsScene::removeItemFromDeletion(mArcPolygonItem);
                mArcPolygonItem->setStrokesGroup(pStrokes);
                pStrokes->addToGroup(mArcPolygonItem);

                // Add the center cross
                foreach(QGraphicsItem* item, mAddedItems){
                    mAddedItems.remove(item);
                    removeItem(item);
                    UBCoreGraphicsScene::removeItemFromDeletion(item);

                    UBGraphicsPolygonItem *crossLine = qgraphicsitem_cast<UBGraphicsPolygonItem *>(item);
                    if (crossLine)
                        crossLine->setStrokesGroup(pStrokes);

                    pStrokes->addToGroup(item);
                }

                mAddedItems.clear();
                mAddedItems << pStrokes;
                addItem(pStrokes);
                mDrawWithCompass = false;

        }
        else if (mCurrentStroke){
                UBGraphicsStrokesGroup* pStrokes = new UBGraphicsStrokesGroup();

                // Remove the strokes that were just drawn here and replace them by a stroke item
                foreach(UBGraphicsPolygonItem* poly, mCurrentStroke->polygons()){
                    mPreviousPolygonItems.removeAll(poly);
                    removeItem(poly);
                    UBCoreGraphicsScene::removeItemFromDeletion(poly);
                    poly->setStrokesGroup(pStrokes);
                    pStrokes->addToGroup(poly);
                }

                // TODO LATER : Generate well pressure-interpolated polygons and create the line group with them

                mAddedItems.clear();
                mAddedItems << pStrokes;
                addItem(pStrokes);

            if (mCurrentStroke->polygons().empty()){
                delete mCurrentStroke;
                mCurrentStroke = 0;
            }
        }
    }

    if (mRemovedItems.size() > 0 || mAddedItems.size() > 0)
    {

        if (mUndoRedoStackEnabled) { //should be deleted after scene own undo stack implemented
            UBGraphicsItemUndoCommand* udcmd = new UBGraphicsItemUndoCommand(this, mRemovedItems, mAddedItems); //deleted by the undoStack

            if(UBApplication::undoStack)
                UBApplication::undoStack->push(udcmd);
        }

        mRemovedItems.clear();
        mAddedItems.clear();
        accepted = true;
    }

    mInputDeviceIsPressed = false;

    setDocumentUpdated();

    if (mCurrentStroke && mCurrentStroke->polygons().empty()){
        delete mCurrentStroke;
    }

    mCurrentStroke = NULL;

    return accepted;
}

void UBGraphicsScene::drawEraser(const QPointF &pPoint, bool isFirstDraw)
{
    qreal eraserWidth = UBSettings::settings()->currentEraserWidth();
    eraserWidth /= UBApplication::boardController->systemScaleFactor();
    eraserWidth /= UBApplication::boardController->currentZoom();

    qreal eraserRadius = eraserWidth / 2;

    // TODO UB 4.x optimize - no need to do that every time we move it
    if (mEraser) {
        mEraser->setRect(QRectF(pPoint.x() - eraserRadius, pPoint.y() - eraserRadius, eraserWidth, eraserWidth));

        if(isFirstDraw) {
          mEraser->show();
        }
    }
}

void UBGraphicsScene::drawPointer(const QPointF &pPoint, bool isFirstDraw)
{
    qreal pointerDiameter = UBSettings::pointerDiameter / UBApplication::boardController->currentZoom();
    qreal pointerRadius = pointerDiameter / 2;

    // TODO UB 4.x optimize - no need to do that every time we move it
    if (mPointer) {
        mPointer->setRect(QRectF(pPoint.x() - pointerRadius,
                                 pPoint.y() - pointerRadius,
                                 pointerDiameter,
                                 pointerDiameter));
        if(isFirstDraw) {
            mPointer->show();
        }
    }
}

// call this function when user release mouse button in Magnifier mode
void UBGraphicsScene::DisposeMagnifierQWidgets()
{
    if(magniferControlViewWidget)
    {
        magniferControlViewWidget->hide();
        magniferControlViewWidget->setParent(0);
        delete magniferControlViewWidget;
        magniferControlViewWidget = NULL;
    }

    if(magniferDisplayViewWidget)
    {
        magniferDisplayViewWidget->hide();
        magniferDisplayViewWidget->setParent(0);
        delete magniferDisplayViewWidget;
        magniferDisplayViewWidget = NULL;
    }

    // some time have crash here on access to app (when call from destructor when close sankore app)
    // so i just add try/catch section here
    try
    {
        UBApplication::app()->restoreOverrideCursor();
    }
    catch (...)
    {
    }

}

void UBGraphicsScene::moveTo(const QPointF &pPoint)
{
    mPreviousPoint = pPoint;
    mPreviousWidth = -1.0;
    mPreviousPolygonItems.clear();
    mArcPolygonItem = 0;
    mDrawWithCompass = false;
}

void UBGraphicsScene::drawLineTo(const QPointF &pEndPoint, const qreal &pWidth, bool bLineStyle)
{
    if (mPreviousWidth == -1.0)
        mPreviousWidth = pWidth;

    UBGraphicsPolygonItem *polygonItem = lineToPolygonItem(QLineF(mPreviousPoint, pEndPoint), pWidth);

    if (!polygonItem->brush().isOpaque())
    {
        // -------------------------------------------------------------------------------------
        // Here we substract the polygons that are overlapping in order to keep the transparency
        // -------------------------------------------------------------------------------------
        for (int i = 0; i < mPreviousPolygonItems.size(); i++)
        {
            UBGraphicsPolygonItem* previous = mPreviousPolygonItems.value(i);
            polygonItem->subtract(previous);
        }
    }

    if (bLineStyle)
    {
        QSetIterator<QGraphicsItem*> itItems(mAddedItems);

        while (itItems.hasNext())
        {
            QGraphicsItem* item = itItems.next();
            removeItem(item);
        }
        mAddedItems.clear();
    }

    mpLastPolygon = polygonItem;
    mAddedItems.insert(polygonItem);

    // Here we add the item to the scene
    addItem(polygonItem);

    if (!mCurrentStroke)
        mCurrentStroke = new UBGraphicsStroke();

    polygonItem->setStroke(mCurrentStroke);

    mPreviousPolygonItems.append(polygonItem);

    if (!bLineStyle)
    {
        mPreviousPoint = pEndPoint;
        mPreviousWidth = pWidth;
    }
}

void UBGraphicsScene::eraseLineTo(const QPointF &pEndPoint, const qreal &pWidth)
{
    const QLineF line(mPreviousPoint, pEndPoint);
    mPreviousPoint = pEndPoint;

    const QPolygonF eraserPolygon = UBGeometryUtils::lineToPolygon(line, pWidth);
    const QRectF eraserBoundingRect = eraserPolygon.boundingRect();

    QPainterPath eraserPath;
    eraserPath.addPolygon(eraserPolygon);

    // Get all the items that are intersecting with the eraser path
    QList<QGraphicsItem*> collidItems = items(eraserBoundingRect, Qt::IntersectsItemBoundingRect);

    QList<UBGraphicsPolygonItem*> intersectedItems;

    typedef QList<QPolygonF> POLYGONSLIST;
    QList<POLYGONSLIST> intersectedPolygons;

    #pragma omp parallel for
    for(int i=0; i<collidItems.size(); i++)
    {
        UBGraphicsPolygonItem *pi = qgraphicsitem_cast<UBGraphicsPolygonItem*>(collidItems[i]);
        if(pi == NULL)
            continue;

        QPainterPath itemPainterPath;
        itemPainterPath.addPolygon(pi->sceneTransform().map(pi->polygon()));

        if (eraserPath.contains(itemPainterPath))
        {
            #pragma omp critical
            {
                // Compete remove item
                intersectedItems << pi;
                intersectedPolygons << QList<QPolygonF>();
            }
        }
        else if (eraserPath.intersects(itemPainterPath))
        {

            QPainterPath newPath = itemPainterPath.subtracted(eraserPath);
            #pragma omp critical
            {
               intersectedItems << pi;
               intersectedPolygons << newPath.simplified().toFillPolygons(pi->sceneTransform().inverted());
            }
        }
    }

    for(int i=0; i<intersectedItems.size(); i++)
    {
        // item who intersects with eraser
        UBGraphicsPolygonItem *intersectedPolygonItem = intersectedItems[i];

        if (!intersectedPolygons[i].empty())
        {
            // intersected polygons generated as QList<QPolygon> QPainterPath::toFillPolygons(),
            // so each intersectedPolygonItem has one or couple of QPolygons who should be removed from it.
            for(int j = 0; j < intersectedPolygons[i].size(); j++)
            {
                // create small polygon from couple of polygons to replace particular erased polygon
                UBGraphicsPolygonItem* polygonItem = new UBGraphicsPolygonItem(intersectedPolygons[i][j], intersectedPolygonItem->parentItem());

                intersectedPolygonItem->copyItemParameters(polygonItem);
                polygonItem->setStroke(intersectedPolygonItem->stroke());
                polygonItem->setStrokesGroup(intersectedPolygonItem->strokesGroup());
                intersectedPolygonItem->strokesGroup()->addToGroup(polygonItem);
                mAddedItems << polygonItem;
            }
        }

        //remove full polygon item for replace it by couple of polygons which creates the same stroke without a part intersects with eraser
         mRemovedItems << intersectedPolygonItem;

        QTransform t;
        bool bApplyTransform = false;
        if (intersectedPolygonItem->strokesGroup())
        {
            if (intersectedPolygonItem->strokesGroup()->parentItem())
            {
                bApplyTransform = true;
                t = intersectedPolygonItem->sceneTransform();
            }
            intersectedPolygonItem->strokesGroup()->removeFromGroup(intersectedPolygonItem);
        }

        removeItem(intersectedPolygonItem);
        if (bApplyTransform)
            intersectedPolygonItem->setTransform(t);


        removeItem(intersectedPolygonItem);

        if (bApplyTransform)
            intersectedPolygonItem->setTransform(t);
    }

    if (!intersectedItems.empty())
        setModified(true);
}

void UBGraphicsScene::drawArcTo(const QPointF& pCenterPoint, qreal pSpanAngle)
{
    mDrawWithCompass = true;
    if (mArcPolygonItem)
    {
        mAddedItems.remove(mArcPolygonItem);
        removeItem(mArcPolygonItem);
        mArcPolygonItem = 0;
    }
    qreal penWidth = UBSettings::settings()->currentPenWidth();
    penWidth /= UBApplication::boardController->systemScaleFactor();
    penWidth /= UBApplication::boardController->currentZoom();

    mArcPolygonItem = arcToPolygonItem(QLineF(pCenterPoint, mPreviousPoint), pSpanAngle, penWidth);
    mArcPolygonItem->setStroke(mCurrentStroke);
    mAddedItems.insert(mArcPolygonItem);
    addItem(mArcPolygonItem);

    setDocumentUpdated();
}

void UBGraphicsScene::setBackground(bool pIsDark, bool pIsCrossed)
{
    bool needRepaint = false;

    if (mDarkBackground != pIsDark)
    {
        mDarkBackground = pIsDark;

        if (mEraser)
        {
            if (mDarkBackground)
            {
                mEraser->setBrush(UBSettings::eraserBrushDarkBackground);
                mEraser->setPen(UBSettings::eraserPenDarkBackground);
            }
            else
            {
                mEraser->setBrush(UBSettings::eraserBrushLightBackground);
                mEraser->setPen(UBSettings::eraserPenLightBackground);
            }
        }

        recolorAllItems();

        needRepaint = true;
        setModified(true);
    }

    if (mCrossedBackground != pIsCrossed)
    {
        mCrossedBackground = pIsCrossed;
        needRepaint = true;
        setModified(true);
    }

    if (needRepaint)
    {
        foreach(QGraphicsView* view, views())
        {
            view->resetCachedContent();
        }
    }
}

void UBGraphicsScene::setBackgroundZoomFactor(qreal zoom)
{
    mZoomFactor = zoom;
}

void UBGraphicsScene::setDrawingMode(bool bModeDesktop)
{
    mIsDesktopMode = bModeDesktop;
}

void UBGraphicsScene::recolorAllItems()
{
    QMap<QGraphicsView*, QGraphicsView::ViewportUpdateMode> previousUpdateModes;
    foreach(QGraphicsView* view, views())
    {
        previousUpdateModes.insert(view, view->viewportUpdateMode());
        view->setViewportUpdateMode(QGraphicsView::NoViewportUpdate);
    }

    bool currentIslight = isLightBackground();
    foreach (QGraphicsItem *item, items()) {
        if (item->type() == UBGraphicsStrokesGroup::Type) {
            UBGraphicsStrokesGroup *curGroup = static_cast<UBGraphicsStrokesGroup*>(item);
//            QColor compareColor =  curGroup->color(currentIslight ? UBGraphicsStrokesGroup::colorOnDarkBackground
//                                                                  : UBGraphicsStrokesGroup::colorOnLightBackground);

//            if (curGroup->color() == compareColor) {
//                QColor newColor = curGroup->color(!currentIslight ? UBGraphicsStrokesGroup::colorOnDarkBackground
//                                                                  : UBGraphicsStrokesGroup::colorOnLightBackground);
//                curGroup->setColor(newColor);
//            }
            UBGraphicsStrokesGroup::colorType reqCol = currentIslight ? UBGraphicsStrokesGroup::colorOnLightBackground
                                                                      : UBGraphicsStrokesGroup::colorOnDarkBackground;

            curGroup->setColor(curGroup->color(reqCol));
        }
    }

    foreach(QGraphicsView* view, views())
    {
        view->setViewportUpdateMode(previousUpdateModes.value(view));
    }
}

UBGraphicsPolygonItem* UBGraphicsScene::lineToPolygonItem(const QLineF &pLine, const qreal &pWidth)
{
    UBGraphicsPolygonItem *polygonItem = new UBGraphicsPolygonItem(pLine, pWidth);

    initPolygonItem(polygonItem);

    return polygonItem;
}

void UBGraphicsScene::initPolygonItem(UBGraphicsPolygonItem* polygonItem)
{
    QColor colorOnDarkBG;
    QColor colorOnLightBG;

    if (UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Marker)
    {
        colorOnDarkBG = UBApplication::boardController->markerColorOnDarkBackground();
        colorOnLightBG = UBApplication::boardController->markerColorOnLightBackground();
    }
    else // settings->stylusTool() == UBStylusTool::Pen + failsafe
    {
        colorOnDarkBG = UBApplication::boardController->penColorOnDarkBackground();
        colorOnLightBG = UBApplication::boardController->penColorOnLightBackground();
    }

    if (mDarkBackground)
    {
        polygonItem->setColor(colorOnDarkBG);
    }
    else
    {
        polygonItem->setColor(colorOnLightBG);
    }

    polygonItem->setColorOnDarkBackground(colorOnDarkBG);
    polygonItem->setColorOnLightBackground(colorOnLightBG);

    polygonItem->setData(UBGraphicsItemData::ItemLayerType, QVariant(UBItemLayerType::Graphic));
}

UBGraphicsPolygonItem* UBGraphicsScene::arcToPolygonItem(const QLineF& pStartRadius, qreal pSpanAngle, qreal pWidth)
{
    QPolygonF polygon = UBGeometryUtils::arcToPolygon(pStartRadius, pSpanAngle, pWidth);

    return polygonToPolygonItem(polygon);
}

UBGraphicsPolygonItem* UBGraphicsScene::polygonToPolygonItem(const QPolygonF pPolygon)
{
    UBGraphicsPolygonItem *polygonItem = new UBGraphicsPolygonItem(pPolygon);

    initPolygonItem(polygonItem);

    return polygonItem;
}

void UBGraphicsScene::hideEraser()
{
    if (mEraser)
        mEraser->hide();
}

void UBGraphicsScene::leaveEvent(QEvent * event)
{
    Q_UNUSED(event);
    hideEraser();
}

UBGraphicsScene* UBGraphicsScene::sceneDeepCopy() const
{
    UBGraphicsScene* copy = new UBGraphicsScene(this->document(), this->mUndoRedoStackEnabled);

    copy->setBackground(this->isDarkBackground(), this->isCrossedBackground());
    copy->setSceneRect(this->sceneRect());

    if (this->mNominalSize.isValid())
        copy->setNominalSize(this->mNominalSize);

    QListIterator<QGraphicsItem*> itItems(this->mFastAccessItems);

    QMap<UBGraphicsStroke*, UBGraphicsStroke*> groupClone;

    while (itItems.hasNext())
    {
        QGraphicsItem* item = itItems.next();
        QGraphicsItem* cloneItem = 0;

        UBItem* ubItem = dynamic_cast<UBItem*>(item);
        UBGraphicsStroke* stroke = dynamic_cast<UBGraphicsStroke*>(item);

        if (ubItem && !stroke)
        {
            cloneItem = dynamic_cast<QGraphicsItem*>(ubItem->deepCopy());
        }

        if (cloneItem)
        {
            copy->addItem(cloneItem);

            if (isBackgroundObject(item))
            {
                copy->setAsBackgroundObject(cloneItem);
            }

            if (this->mTools.contains(item))
            {
                copy->mTools << cloneItem;
            }

            UBGraphicsPolygonItem* polygon = dynamic_cast<UBGraphicsPolygonItem*>(item);

            if(polygon)
            {
                UBGraphicsStroke* stroke = dynamic_cast<UBGraphicsStroke*>(item->parentItem());

                if (stroke)
                {
                    UBGraphicsStroke* cloneStroke = groupClone.value(stroke);

                    if (!cloneStroke)
                    {
                        cloneStroke = stroke->deepCopy();
                        groupClone.insert(stroke, cloneStroke);
                    }

                    polygon->setStroke(cloneStroke);
                }
            }
        }
    }

    // TODO UB 4.7 ... complete all members ?

    return copy;
}

UBItem* UBGraphicsScene::deepCopy() const
{
    return sceneDeepCopy();
}

void UBGraphicsScene::clearContent(clearCase pCase)
{
    QSet<QGraphicsItem*> removedItems;
    UBGraphicsItemUndoCommand::GroupDataTable groupsMap;

    switch (pCase) {
    case clearBackground :
        if(mBackgroundObject){
            mBackgroundObjectDisposition = Center; // Issue 1684 - CFA - 20131128
            mDocument->setHasDefaultImageBackground(false); // Issue 1684 - ALTI/AOU - 20131210
            mBackgroundObjectUrl = QUrl();
            removeItem(mBackgroundObject);
            removedItems << mBackgroundObject;
        }
        break;

    case clearItemsAndAnnotations :
    case clearItems :
    case clearAnnotations :
        // Issue 1569 - CFA - 20131023 : undo "erase multi-selection"
        UBApplication::undoStack->beginMacro("remove items");
        foreach(QGraphicsItem* item, items()) {

            bool isGroup = item->type() == UBGraphicsGroupContainerItem::Type;
            bool isStrokesGroup = item->type() == UBGraphicsStrokesGroup::Type;

            UBGraphicsGroupContainerItem *itemGroup = item->parentItem()
                                                      ? qgraphicsitem_cast<UBGraphicsGroupContainerItem*>(item->parentItem())
                                                      : 0;
            UBGraphicsItemDelegate *curDelegate = UBGraphicsItem::Delegate(item);
            if (!curDelegate) {
                continue;
            }

            bool shouldDelete = false;

            switch (static_cast<int>(pCase)) {
            case clearAnnotations :
                shouldDelete = isStrokesGroup;
                break;
            case clearItems :
                shouldDelete = !isGroup && !isBackgroundObject(item) && !isStrokesGroup;
                break;
            case clearItemsAndAnnotations:
                shouldDelete = !isGroup && !isBackgroundObject(item);
                break;
            }

            if(shouldDelete) {
                if (itemGroup) {
                    itemGroup->removeFromGroup(item);

                    groupsMap.insert(itemGroup, UBGraphicsItem::getOwnUuid(item));
                    if (itemGroup->childItems().count() == 1) {
                        groupsMap.insert(itemGroup, UBGraphicsItem::getOwnUuid(itemGroup->childItems().first()));
                        QGraphicsItem *lastItem = itemGroup->childItems().first();
                        bool isSelected = itemGroup->isSelected();
                        itemGroup->destroy(false);
                        lastItem->setSelected(isSelected);
                    }
                    itemGroup->Delegate()->update();
                }

                curDelegate->remove(false);
                removedItems << item;
            }
        }
        UBApplication::undoStack->endMacro();// Fin Issue 1569 - CFA - 20131023
        break;
    }

    // force refresh, QT is a bit lazy and take a lot of time (nb item ^2 ?) to trigger repaint
    update(sceneRect());

    if (mUndoRedoStackEnabled) { //should be deleted after scene own undo stack implemented

        UBGraphicsItemUndoCommand* uc = new UBGraphicsItemUndoCommand(this, removedItems, QSet<QGraphicsItem*>(), groupsMap);
        UBApplication::undoStack->push(uc);
    }

    if (pCase == clearBackground) {
        mBackgroundObject = 0;
    }

    setDocumentUpdated();
}

UBGraphicsPixmapItem* UBGraphicsScene::addPixmap(const QPixmap& pPixmap, QGraphicsItem* replaceFor, const QPointF& pPos, qreal pScaleFactor, bool pUseAnimation)
{
    UBGraphicsPixmapItem* pixmapItem = new UBGraphicsPixmapItem();

    pixmapItem->setFlag(QGraphicsItem::ItemIsMovable, true);
    pixmapItem->setFlag(QGraphicsItem::ItemIsSelectable, true);

    pixmapItem->setPixmap(pPixmap);

    QPointF half(pPixmap.width() * pScaleFactor / 2, pPixmap.height()  * pScaleFactor / 2);
    pixmapItem->setPos(pPos - half);

    addItem(pixmapItem);

    if (mUndoRedoStackEnabled) { //should be deleted after scene own undo stack implemented
        UBGraphicsItemUndoCommand* uc = new UBGraphicsItemUndoCommand(this, replaceFor, pixmapItem);
        UBApplication::undoStack->push(uc);
    }

    pixmapItem->scale(pScaleFactor, pScaleFactor);

    if (pUseAnimation)
    {
        pixmapItem->setOpacity(0);

        QPropertyAnimation *animation = new QPropertyAnimation(pixmapItem, "opacity");
        animation->setDuration(1000);
        animation->setStartValue(0.0);
        animation->setEndValue(1.0);

        animation->start();
    }

    pixmapItem->show();
    setDocumentUpdated();

    return pixmapItem;
}

void UBGraphicsScene::textUndoCommandAdded(UBGraphicsTextItem *textItem)
{
    if (mUndoRedoStackEnabled) { //should be deleted after scene own undo stack implemented
        UBGraphicsTextItemUndoCommand* uc = new UBGraphicsTextItemUndoCommand(textItem);
        UBApplication::undoStack->push(uc);
    }
}
UBGraphicsMediaItem* UBGraphicsScene::addMedia(const QUrl& pMediaFileUrl, bool shouldPlayAsap, const QPointF& pPos)
{
    qDebug() << pMediaFileUrl.toLocalFile();
    if (!QFile::exists(pMediaFileUrl.toLocalFile()))
    if (!QFile::exists(pMediaFileUrl.toString()))
        return NULL;

    UBGraphicsMediaItem* mediaItem = new UBGraphicsMediaItem(pMediaFileUrl);
    if(mediaItem){
        connect(UBApplication::boardController, SIGNAL(activeSceneChanged()), mediaItem, SLOT(activeSceneChanged()));
    }

    mediaItem->setPos(pPos);

    mediaItem->setFlag(QGraphicsItem::ItemIsMovable, true);
    mediaItem->setFlag(QGraphicsItem::ItemIsSelectable, true);

    addItem(mediaItem);

    mediaItem->show();

    if (mUndoRedoStackEnabled) { //should be deleted after scene own undo stack implemented
        UBGraphicsItemUndoCommand* uc = new UBGraphicsItemUndoCommand(this, 0, mediaItem);
        UBApplication::undoStack->push(uc);
    }

    mediaItem->mediaObject()->play();

    if (!shouldPlayAsap)
    {
        mediaItem->mediaObject()->pause();
        mediaItem->mediaObject()->seek(0);
    }

    setDocumentUpdated();

    return mediaItem;
}

UBGraphicsMediaItem* UBGraphicsScene::addVideo(const QUrl& pVideoFileUrl, bool shouldPlayAsap, const QPointF& pPos)
{
   return addMedia(pVideoFileUrl, shouldPlayAsap, pPos);
}

UBGraphicsMediaItem* UBGraphicsScene::addAudio(const QUrl& pAudioFileUrl, bool shouldPlayAsap, const QPointF& pPos)
{
   return addMedia(pAudioFileUrl, shouldPlayAsap, pPos);
}

UBGraphicsWidgetItem* UBGraphicsScene::addWidget(const QUrl& pWidgetUrl, const QPointF& pPos)
{
    int widgetType = UBGraphicsWidgetItem::widgetType(pWidgetUrl);

    if(widgetType == UBWidgetType::Apple)
    {
        return addAppleWidget(pWidgetUrl, pPos);
    }
    else if(widgetType == UBWidgetType::W3C)
    {
        return addW3CWidget(pWidgetUrl, pPos);
    }
    else
    {
        qDebug() << "UBGraphicsScene::addWidget: Unknown widget Type";
        return 0;
    }
}

UBGraphicsAppleWidgetItem* UBGraphicsScene::addAppleWidget(const QUrl& pWidgetUrl, const QPointF& pPos)
{
    UBGraphicsAppleWidgetItem *appleWidget = new UBGraphicsAppleWidgetItem(pWidgetUrl);

    addGraphicsWidget(appleWidget, pPos);

    return appleWidget;
}

//N/C - NNE - 20140415 : add the source url parameter
UBGraphicsW3CWidgetItem* UBGraphicsScene::addW3CWidget(const QUrl& pWidgetUrl, const QPointF& pPos, const QUrl& sourceUrl)
{
    UBGraphicsW3CWidgetItem *w3CWidget = new UBGraphicsW3CWidgetItem(pWidgetUrl, 0, sourceUrl);

    addGraphicsWidget(w3CWidget, pPos);

    return w3CWidget;
}

void UBGraphicsScene::addGraphicsWidget(UBGraphicsWidgetItem* graphicsWidget, const QPointF& pPos)
{
    graphicsWidget->setFlag(QGraphicsItem::ItemIsSelectable, true);

    addItem(graphicsWidget);

    qreal ssf = 1 / UBApplication::boardController->systemScaleFactor();

    graphicsWidget->scale(ssf, ssf);

    graphicsWidget->setPos(QPointF(pPos.x() - graphicsWidget->boundingRect().width() / 2,
        pPos.y() - graphicsWidget->boundingRect().height() / 2));

    if (graphicsWidget->canBeContent())
    {
        graphicsWidget->loadMainHtml();
        graphicsWidget->showLoadingMessage();

        graphicsWidget->setSelected(true);
        if (mUndoRedoStackEnabled) { //should be deleted after scene own undo stack implemented
            UBGraphicsItemUndoCommand* uc = new UBGraphicsItemUndoCommand(this, 0, graphicsWidget);
            UBApplication::undoStack->push(uc);
        }

        setDocumentUpdated();
    }
    else
    {
        UBApplication::boardController->moveGraphicsWidgetToControlView(graphicsWidget);
    }

    UBApplication::boardController->controlView()->setFocus();
}



UBGraphicsW3CWidgetItem* UBGraphicsScene::addOEmbed(const QUrl& pContentUrl, const QPointF& pPos)
{
    QStringList widgetPaths = UBPersistenceManager::persistenceManager()->allWidgets(UBSettings::settings()->applicationApplicationsLibraryDirectory());

    UBGraphicsW3CWidgetItem *widget = 0;

    foreach(QString widgetPath, widgetPaths)
    {
        if (widgetPath.contains("Sel video"))
        {
            widget = addW3CWidget(QUrl::fromLocalFile(widgetPath), pPos);

            if (widget)
            {
                widget->setPreference("oembedUrl", pContentUrl.toString());
                setDocumentUpdated();
                break;
            }
        }
    }

    return widget;
}

UBGraphicsGroupContainerItem *UBGraphicsScene::createGroup(QList<QGraphicsItem *> items)
{
    UBGraphicsGroupContainerItem *groupItem = new UBGraphicsGroupContainerItem();

    addItem(groupItem);
    foreach (QGraphicsItem *item, items) {
        if (item->type() == UBGraphicsGroupContainerItem::Type) {
            QList<QGraphicsItem*> childItems = item->childItems();
            UBGraphicsGroupContainerItem *currentGroup = dynamic_cast<UBGraphicsGroupContainerItem*>(item);
            if (currentGroup) {
                currentGroup->destroy();
            }
            foreach (QGraphicsItem *chItem, childItems) {
                groupItem->addToGroup(chItem);
                mFastAccessItems.removeAll(chItem);
            }
        } else {
            groupItem->addToGroup(item);
            mFastAccessItems.removeAll(item);
        }
    }

    groupItem->setVisible(true);
    groupItem->setFocus();

    if (mUndoRedoStackEnabled) { //should be deleted after scene own undo stack implemented
        UBGraphicsItemGroupUndoCommand* uc = new UBGraphicsItemGroupUndoCommand(this, groupItem);
        UBApplication::undoStack->push(uc);
    }

    setDocumentUpdated();

    return groupItem;
}

void UBGraphicsScene::addGroup(UBGraphicsGroupContainerItem *groupItem)
{
    addItem(groupItem);
    for (int i = 0; i < groupItem->childItems().count(); i++)
    {
        QGraphicsItem *it = qgraphicsitem_cast<QGraphicsItem *>(groupItem->childItems().at(i));
        if (it)
        {
             mFastAccessItems.removeAll(it);
        }
    }

    groupItem->setVisible(true);
    groupItem->setFocus();

    if (groupItem->uuid().isNull()) {
        groupItem->setUuid(QUuid::createUuid());
    }

    if (mUndoRedoStackEnabled) { //should be deleted after scene own undo stack implemented
        UBGraphicsItemUndoCommand* uc = new UBGraphicsItemUndoCommand(this, 0, groupItem);
        UBApplication::undoStack->push(uc);
    }

    setDocumentUpdated();
}

UBGraphicsSvgItem* UBGraphicsScene::addSvg(const QUrl& pSvgFileUrl, const QPointF& pPos, const QByteArray pData)
{
    QString path = pSvgFileUrl.toLocalFile();

    UBGraphicsSvgItem *svgItem;
    if (pData.isNull())
        svgItem = new UBGraphicsSvgItem(path);
    else
        svgItem = new UBGraphicsSvgItem(pData);

    svgItem->setFlag(QGraphicsItem::ItemIsMovable, true);
    svgItem->setFlag(QGraphicsItem::ItemIsSelectable, true);

    qreal sscale = 1 / UBApplication::boardController->systemScaleFactor();
    svgItem->scale(sscale, sscale);

    QPointF half(svgItem->boundingRect().width() / 2, svgItem->boundingRect().height() / 2);
    svgItem->setPos(pPos - half);
    svgItem->show();

    addItem(svgItem);

    if (mUndoRedoStackEnabled) { //should be deleted after scene own undo stack implemented
        UBGraphicsItemUndoCommand* uc = new UBGraphicsItemUndoCommand(this, 0, svgItem);
        UBApplication::undoStack->push(uc);
    }

    setDocumentUpdated();

    return svgItem;
}

UBGraphicsTextItem* UBGraphicsScene::addText(const QString& pString, const QPointF& pTopLeft)
{
    return addTextWithFont(pString, pTopLeft, UBSettings::settings()->fontPixelSize()
            , UBSettings::settings()->fontFamily(), UBSettings::settings()->isBoldFont()
            , UBSettings::settings()->isItalicFont());
}

UBGraphicsTextItem* UBGraphicsScene::textForObjectName(const QString& pString, const QString& objectName)
{
    UBGraphicsTextItem* textItem = 0;
    bool found = false;
    //looking for a previous such item text
    for(int i=0; i < mFastAccessItems.count() && !found ; i += 1){
        UBGraphicsTextItem* currentItem = dynamic_cast<UBGraphicsTextItem*>(mFastAccessItems.at(i));
        if(currentItem && (currentItem->objectName() == objectName || currentItem->toPlainText() == pString)){
            // The second condition is necessary because the object name isn't stored. On reopeining the file we
            // need another rule than the objectName
            textItem = currentItem;
            found=true;
            if(currentItem->objectName() != objectName)
                textItem->setObjectName(objectName);
        }
    }

    if(!textItem){
        textItem = addTextWithFont(pString,QPointF(0,0) ,72,UBSettings::settings()->fontFamily(),true,false);
        textItem->setObjectName(objectName);
        textItem->setData(UBGraphicsItemData::ItemEditable,QVariant(false));
        textItem->adjustSize();
        textItem->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        textItem->setPlainText(pString);
    }
    else{
        textItem->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        if (pString == textItem->toPlainText())
            return textItem;

        QTextCursor curCursor = textItem->textCursor();
        QFont font = textItem->font();
        QColor color = curCursor.charFormat().foreground().color();

        textItem->setPlainText(pString);
        textItem->clearFocus();
        textItem->setFont(font);


        QTextCharFormat format;
        format.setForeground(QBrush(color));
        curCursor.mergeCharFormat(format);
        textItem->setTextCursor(curCursor);
//        textItem->setSelected(true);
        textItem->contentsChanged();

    }

    textItem->clearFocus();

    return textItem;
}

UBGraphicsTextItem* UBGraphicsScene::addTextWithFont(const QString& pString, const QPointF& pTopLeft
            , int pointSize, const QString& fontFamily, bool bold, bool italic)
{
    UBGraphicsTextItem *textItem = new UBGraphicsTextItem();
    textItem->setPlainText(pString);

    QFont font = textItem->font();

    if (fontFamily == "")
    {
        font = QFont(UBSettings::settings()->fontFamily());
    }
    else
    {
        font = QFont(fontFamily);
    }

    if (pointSize < 1)
    {
        font.setPixelSize(UBSettings::settings()->fontPixelSize());
    }
    else
    {
        font.setPointSize(pointSize);
    }

    font.setBold(bold);
    font.setItalic(italic);
    textItem->setFont(font);

    QFontMetrics fi(font);
    QRect br = fi.boundingRect(pString);

    textItem->setTextWidth(qMax((qreal)br.width() + 50, (qreal)200));
    textItem->setTextHeight(br.height());

    addItem(textItem);

    textItem->setPos(pTopLeft);

    textItem->show();

    if (mUndoRedoStackEnabled) { //should be deleted after scene own undo stack implemented
        UBGraphicsItemUndoCommand* uc = new UBGraphicsItemUndoCommand(this, 0, textItem);
        UBApplication::undoStack->push(uc);
    }

    connect(textItem, SIGNAL(textUndoCommandAdded(UBGraphicsTextItem *)), this, SLOT(textUndoCommandAdded(UBGraphicsTextItem *)));

    textItem->setSelected(true);
    textItem->setFocus();

    setDocumentUpdated();

    return textItem;
}

UBGraphicsTextItem *UBGraphicsScene::addTextHtml(const QString &pString, const QPointF& pTopLeft)
{
    QString cleanString = UBTextTools::cleanHtml(pString);
    if(!cleanString.length()){
        qDebug() << "Cleaning the string leads to an empty string";
        cleanString = pString;
    }
    UBGraphicsTextItem *textItem = new UBGraphicsTextItem();

    textItem->setPlainText("");
    textItem->setHtml(cleanString);

    addItem(textItem);
    textItem->show();

    if (mUndoRedoStackEnabled) { //should be deleted after scene own undo stack implemented
        UBGraphicsItemUndoCommand* uc = new UBGraphicsItemUndoCommand(this, 0, textItem);
        UBApplication::undoStack->push(uc);
    }

    connect(textItem, SIGNAL(textUndoCommandAdded(UBGraphicsTextItem *)),
            this,     SLOT(textUndoCommandAdded(UBGraphicsTextItem *)));

    textItem->setFocus();

    setDocumentUpdated();
    textItem->setPos(pTopLeft);

    return textItem;
}

void UBGraphicsScene::addItem(QGraphicsItem* item)
{
    UBCoreGraphicsScene::addItem(item);

    UBGraphicsItem::assignZValue(item, mZLayerController->generateZLevel(item));

    mFastAccessItems << item;

    //CFA
    addShapeToUndoStack(item);
}

void UBGraphicsScene::addShapeToUndoStack(QGraphicsItem* item)
{
    //CFA - TEST UNDO
    UBAbstractGraphicsItem * shape = dynamic_cast<UBAbstractGraphicsItem*>(item);
    if (shape)
    {
        mAddedItems.insert(item);
    }
}

void UBGraphicsScene::removeShapeToUndoStack(QGraphicsItem* item)
{
    //CFA - TEST UNDO
    UBAbstractGraphicsItem * shape = dynamic_cast<UBAbstractGraphicsItem*>(item);
    if (shape)
    {
        mRemovedItems.insert(item);
    }
}

void UBGraphicsScene::addItems(const QSet<QGraphicsItem*>& items)
{
    foreach(QGraphicsItem* item, items) {
        UBCoreGraphicsScene::addItem(item);
        UBGraphicsItem::assignZValue(item, mZLayerController->generateZLevel(item));
    }

    mFastAccessItems += items.toList();
}

void UBGraphicsScene::removeItem(QGraphicsItem* item)
{
    item->setSelected(false);

    UBCoreGraphicsScene::removeItem(item);

    UBApplication::boardController->freezeW3CWidget(item, true);

    mFastAccessItems.removeAll(item);

    //CFA
    removeShapeToUndoStack(item);

    /* delete the item if it is cache to allow its reinstanciation, because Cache implements design pattern Singleton. */
    if (dynamic_cast<UBGraphicsCache*>(item))
        UBCoreGraphicsScene::deleteItem(item);
}

void UBGraphicsScene::removeItems(const QSet<QGraphicsItem*>& items)
{
    foreach(QGraphicsItem* item, items) {
        UBCoreGraphicsScene::removeItem(item);
    }

    foreach(QGraphicsItem* item, items)
        mFastAccessItems.removeAll(item);
}

void UBGraphicsScene::deselectAllItems()
{
    foreach(QGraphicsItem *gi, selectedItems ())
    {
        gi->setSelected(false);

        //issue 1554 - NNE - 20131010
        UBGraphicsTextItem* gti = dynamic_cast<UBGraphicsTextItem*>(gi);
        if(gti){
            gti->activateTextEditor(false);
        }
        //issue 1554 - NNE - 20131010 : END

        //EV-7 - NNE - 20140206
        if(UBShapeFactory::isShape(gi)){
            UBShapeFactory::desactivateEditionMode(gi);
        }
    }
}

//issue 1554 - NNE - 20131010
void UBGraphicsScene::deselectAllItemsExcept(QGraphicsItem* gti)
{
    foreach(QGraphicsItem *gi, selectedItems ())
    {
        if(gi != gti){
            gi->setSelected(false);

            UBGraphicsTextItem* g = dynamic_cast<UBGraphicsTextItem*>(gi);
            if(g){
                g->activateTextEditor(false);
            }

            //EV-7 - NNE - 20140206
            if(UBShapeFactory::isShape(gi)){
                UBShapeFactory::desactivateEditionMode(gi);
            }

        }

    }
}
//issue 1554 - NNE - 20131010 : END

bool UBGraphicsScene::isEmpty() const
{
    //Issue NC - CFA - 20140417 : there is always at least 2 objects when the scene is Empty : eraser + pointer.
    //To make sure that no unexpected behavior will be thrown if this test is done earlier, we use the "lte operator"
    return items().count() <= 2;
}



QUrl UBGraphicsScene::backgroundObjectUrl()
{
    return mBackgroundObjectUrl;
}

void UBGraphicsScene::setBackgroundObjectUrl(QUrl url)
{
    mBackgroundObjectUrl = url;
}

// Issue 1684 - CFA - 20131119 : ici pour calculer le résultat final
QGraphicsItem* UBGraphicsScene::setAsBackgroundObject(QGraphicsItem* item, bool pAdaptTransformation, bool pExpand, UBFeatureBackgroundDisposition disposition)
{
    if (mBackgroundObject)
    {
        removeItem(mBackgroundObject);
        mBackgroundObject = 0;
    }

    // Issue 1684 - CFA - 20131128
    mBackgroundObjectDisposition = disposition;

    if (item)
    {
        item->setFlag(QGraphicsItem::ItemIsSelectable, false);
        item->setFlag(QGraphicsItem::ItemIsMovable, false);
        item->setAcceptedMouseButtons(Qt::NoButton);
        item->setData(UBGraphicsItemData::ItemLayerType, UBItemLayerType::FixedBackground);

        if (pAdaptTransformation)
        {
            item = scaleToFitDocumentSize(item, true, 0, pExpand, disposition);
        }

        if (item->scene() != this)
            addItem(item);

        mZLayerController->setLayerType(item, itemLayerType::BackgroundItem);
        UBGraphicsItem::assignZValue(item, mZLayerController->generateZLevel(item));

        mBackgroundObject = item;

    }

    return item;
}

QRectF UBGraphicsScene::normalizedSceneRect(qreal ratio)
{

    QRectF normalizedRect(nominalSize().width() / -2, nominalSize().height() / -2,
        nominalSize().width(), nominalSize().height());

    foreach(QGraphicsItem* gi, mFastAccessItems)
    {
        if(gi && gi->isVisible() && !mTools.contains(gi))
        {
            normalizedRect = normalizedRect.united(gi->sceneBoundingRect());
        }
    }

    if (ratio > 0.0)
    {
        qreal normalizedRectRatio = normalizedRect.width() / normalizedRect.height();

        if (normalizedRectRatio > ratio)
        {
            //the normalized rect is too wide, we increase height
            qreal newHeight = normalizedRect.width() / ratio;
            qreal offset = (newHeight - normalizedRect.height()) / 2;
            normalizedRect.setY(normalizedRect.y() - offset);
            normalizedRect.setHeight(newHeight);
        }
        else if (normalizedRectRatio < ratio)
        {
            //the normalized rect is too high, we increase the width
            qreal newWidth = normalizedRect.height() * ratio;
            qreal offset = (newWidth - normalizedRect.width()) / 2;
            normalizedRect.setX(normalizedRect.x() - offset);
            normalizedRect.setWidth(newWidth);
        }
    }

    return normalizedRect;
}

QGraphicsItem *UBGraphicsScene::itemForUuid(QUuid uuid)
{
    QGraphicsItem *result = 0;
    QString ui = uuid.toString();

    //simple search before implementing container for fast access
    foreach (QGraphicsItem *item, items()) {
        if (UBGraphicsScene::getPersonalUuid(item) == uuid && !uuid.isNull()) {
            result = item;
        }
    }

    return result;
}

void UBGraphicsScene::setDocument(UBDocumentProxy* pDocument)
{
    if (pDocument != mDocument)
    {
        if (mDocument)
        {
            setModified(true);
        }

        mDocument = pDocument;
        setParent(pDocument);
    }
}

// Issue 1684 - CFA - 20131127 : on adapte l'item à la disposition souhaitée
QGraphicsItem* UBGraphicsScene::scaleToFitDocumentSize(QGraphicsItem* item, bool center, int margin, bool expand, UBFeatureBackgroundDisposition disposition)
{
    int maxWidth = 0;
    int maxHeight = 0;

    // On reinit les transform de l'item, car dans cette fonction, on va recalculer son scale et sa position
    item->setTransform(QTransform()); // Issue 1684 - ALTI/AOU - 20131210

    maxWidth = mNominalSize.width() - (margin * 2);
    maxHeight = mNominalSize.height() - (margin * 2);

    QRectF size = item->sceneBoundingRect();

    if (disposition == Adjust)
    {
        qreal ratio = qMin(maxWidth / size.width(), maxHeight / size.height());

        item->scale(ratio, ratio);

        if(center)
        {
            item->setPos(item->sceneBoundingRect().width() / -2.0,
                item->sceneBoundingRect().height() / -2.0);
        }
    }
    else if (disposition == Mosaic)
    {
        if(center)
        {
            item->setPos(item->sceneBoundingRect().width() / -2.0,
                item->sceneBoundingRect().height() / -2.0);
        }
    }
    else if (disposition == Fill)
    {
        qreal ratio = qMax(maxWidth / size.width(), maxHeight / size.height());

        item->scale(ratio, ratio);

        if(center)
        {
            item->setPos(item->sceneBoundingRect().width() / -2.0,
                item->sceneBoundingRect().height() / -2.0);
        }
    }
    else if (disposition == Extend)
    {
        item->scale(maxWidth / size.width(),maxHeight / size.height());

        if(center)
        {
            item->setPos(item->sceneBoundingRect().width() / -2.0,
                item->sceneBoundingRect().height() / -2.0);
        }

    }
    else //Center
    {
        //Issue 1684 - CFA - 20140116 : rescale de l'image dans le cas où elle dépasse (ici il faudrait recevoir '!center' dans le cas d'un drag'n'drop)
        if (expand || size.width() > maxWidth || size.height() > maxHeight)
        {
            qreal ratio = qMin(maxWidth / size.width(), maxHeight / size.height());

            item->scale(ratio, ratio);           
        }
        if (center)
            item->setPos(item->sceneBoundingRect().width() / -2.0, item->sceneBoundingRect().height() / -2.0);
    }

    return item;
}
// Fin Issue 1684 - CFA - 20131127

void UBGraphicsScene::addRuler(QPointF center)
{
    UBGraphicsRuler* ruler = new UBGraphicsRuler(); // mem : owned and destroyed by the scene
    mTools << ruler;
    QRectF rect = ruler->rect();
    ruler->setRect(center.x() - rect.width()/2, center.y() - rect.height()/2, rect.width(), rect.height());

    ruler->setData(UBGraphicsItemData::ItemLayerType, QVariant(UBItemLayerType::Tool));

    addItem(ruler);

    ruler->setVisible(true);
}

void UBGraphicsScene::addProtractor(QPointF center)
{
    // Protractor

    UBGraphicsProtractor* protractor = new UBGraphicsProtractor(); // mem : owned and destroyed by the scene
    mTools << protractor;

    protractor->setData(UBGraphicsItemData::ItemLayerType, QVariant(UBItemLayerType::Tool));

    addItem(protractor);

    QPointF itemSceneCenter = protractor->sceneBoundingRect().center();
    protractor->moveBy(center.x() - itemSceneCenter.x(), center.y() - itemSceneCenter.y());

    protractor->setVisible(true);
}

void UBGraphicsScene::addTriangle(QPointF center)
{
    // Triangle

    UBGraphicsTriangle* triangle = new UBGraphicsTriangle(); // mem : owned and destroyed by the scene
    mTools << triangle;

    triangle->setData(UBGraphicsItemData::ItemLayerType, QVariant(UBItemLayerType::Tool));

    addItem(triangle);

    QPointF itemSceneCenter = triangle->sceneBoundingRect().center();
    triangle->moveBy(center.x() - itemSceneCenter.x(), center.y() - itemSceneCenter.y());

    triangle->setVisible(true);
}

void UBGraphicsScene::addMagnifier(UBMagnifierParams params)
{
    // can have only one magnifier at one time
    if(magniferControlViewWidget) return;

    QWidget *cContainer = (QWidget*)(UBApplication::boardController->controlContainer());
    QGraphicsView *cView = (QGraphicsView*)UBApplication::boardController->controlView();
    QGraphicsView *dView = (QGraphicsView*)UBApplication::boardController->displayView();

    QPoint dvZeroPoint = dView->mapToGlobal(QPoint(0,0));

    int cvW = cView->width();
    int dvW = dView->width();
    qreal wCoeff = (qreal)dvW / (qreal)cvW;

    int cvH = cView->height();
    int dvH = dView->height();
    qreal hCoeff = (qreal)dvH / (qreal)cvH;

    QPoint ccPoint(params.x,params.y);
    QPoint globalPoint = cContainer->mapToGlobal(ccPoint);
    QPoint cvPoint = cView->mapFromGlobal(globalPoint);
    QPoint dvPoint( cvPoint.x() * wCoeff + dvZeroPoint.x(), cvPoint.y() * hCoeff + dvZeroPoint.y());

    magniferControlViewWidget = new UBMagnifier((QWidget*)(UBApplication::boardController->controlContainer()), true);
    magniferControlViewWidget->setGrabView((QGraphicsView*)UBApplication::boardController->controlView());
    magniferControlViewWidget->setMoveView((QWidget*)(UBApplication::boardController->controlContainer()));
    magniferControlViewWidget->setSize(params.sizePercentFromScene);
    magniferControlViewWidget->setZoom(params.zoom);

    magniferDisplayViewWidget = new UBMagnifier((QWidget*)(UBApplication::boardController->displayView()), false);
    magniferDisplayViewWidget->setGrabView((QGraphicsView*)UBApplication::boardController->controlView());
    magniferDisplayViewWidget->setMoveView((QGraphicsView*)UBApplication::boardController->displayView());
    magniferDisplayViewWidget->setSize(params.sizePercentFromScene);
    magniferDisplayViewWidget->setZoom(params.zoom);

    magniferControlViewWidget->grabNMove(globalPoint, globalPoint, true);
    magniferDisplayViewWidget->grabNMove(globalPoint, dvPoint, true);
    magniferControlViewWidget->show();
    magniferDisplayViewWidget->show();

    connect(magniferControlViewWidget, SIGNAL(magnifierMoved_Signal(QPoint)), this, SLOT(moveMagnifier(QPoint)));
    connect(magniferControlViewWidget, SIGNAL(magnifierClose_Signal()), this, SLOT(closeMagnifier()));
    connect(magniferControlViewWidget, SIGNAL(magnifierZoomIn_Signal()), this, SLOT(zoomInMagnifier()));
    connect(magniferControlViewWidget, SIGNAL(magnifierZoomOut_Signal()), this, SLOT(zoomOutMagnifier()));
    connect(magniferControlViewWidget, SIGNAL(magnifierDrawingModeChange_Signal(int)), this, SLOT(changeMagnifierMode(int)));
    connect(magniferControlViewWidget, SIGNAL(magnifierResized_Signal(qreal)), this, SLOT(resizedMagnifier(qreal)));

    setModified(true);
}

void UBGraphicsScene::moveMagnifier()
{
   if (magniferControlViewWidget)
   {
       QPoint magnifierPos = QPoint(magniferControlViewWidget->pos().x() + magniferControlViewWidget->size().width() / 2, magniferControlViewWidget->pos().y() + magniferControlViewWidget->size().height() / 2 );
       moveMagnifier(magnifierPos, true);
       setModified(true);
   }
}

void UBGraphicsScene::moveMagnifier(QPoint newPos, bool forceGrab)
{
    QWidget *cContainer = (QWidget*)(UBApplication::boardController->controlContainer());
    QGraphicsView *cView = (QGraphicsView*)UBApplication::boardController->controlView();
    QGraphicsView *dView = (QGraphicsView*)UBApplication::boardController->displayView();

    QPoint dvZeroPoint = dView->mapToGlobal(QPoint(0,0));

    int cvW = cView->width();
    int dvW = dView->width();
    qreal wCoeff = (qreal)dvW / (qreal)cvW;

    int cvH = cView->height();
    int dvH = dView->height();
    qreal hCoeff = (qreal)dvH / (qreal)cvH;

    QPoint globalPoint = cContainer->mapToGlobal(newPos);
    QPoint cvPoint = cView->mapFromGlobal(globalPoint);
    QPoint dvPoint( cvPoint.x() * wCoeff + dvZeroPoint.x(), cvPoint.y() * hCoeff + dvZeroPoint.y());

    magniferControlViewWidget->grabNMove(globalPoint, globalPoint, forceGrab, false);
    magniferDisplayViewWidget->grabNMove(globalPoint, dvPoint, forceGrab, true);

    setModified(true);
}

void UBGraphicsScene::closeMagnifier()
{
    DisposeMagnifierQWidgets();
    setModified(true);
}

void UBGraphicsScene::zoomInMagnifier()
{
    if(magniferControlViewWidget->params.zoom < 8)
    {
        magniferControlViewWidget->setZoom(magniferControlViewWidget->params.zoom + 0.5);
        magniferDisplayViewWidget->setZoom(magniferDisplayViewWidget->params.zoom + 0.5);
    }
}

void UBGraphicsScene::zoomOutMagnifier()
{
    if(magniferControlViewWidget->params.zoom > 1)
    {
        magniferControlViewWidget->setZoom(magniferControlViewWidget->params.zoom - 0.5);
        magniferDisplayViewWidget->setZoom(magniferDisplayViewWidget->params.zoom - 0.5);
        setModified(true);
    }
}

void UBGraphicsScene::changeMagnifierMode(int mode)
{
    if(magniferControlViewWidget)
        magniferControlViewWidget->setDrawingMode(mode);
    if(magniferDisplayViewWidget)
        magniferDisplayViewWidget->setDrawingMode(mode);
}

void UBGraphicsScene::resizedMagnifier(qreal newPercent)
{
    if(newPercent > 18 && newPercent < 50)
    {
        magniferControlViewWidget->setSize(newPercent);
        magniferControlViewWidget->grabPoint();
        magniferDisplayViewWidget->setSize(newPercent);
        magniferDisplayViewWidget->grabPoint();
        setModified(true);
    }
}

void UBGraphicsScene::addCompass(QPointF center)
{
    UBGraphicsCompass* compass = new UBGraphicsCompass(); // mem : owned and destroyed by the scene
    mTools << compass;
    addItem(compass);

    QRectF rect = compass->rect();
    compass->setRect(center.x() - rect.width() / 2, center.y() - rect.height() / 2, rect.width(), rect.height());

    compass->setData(UBGraphicsItemData::ItemLayerType, QVariant(UBItemLayerType::Tool));

    compass->setVisible(true);
}

void UBGraphicsScene::addAristo(QPointF center)
{
    UBGraphicsAristo* aristo = new UBGraphicsAristo();
    mTools << aristo;

    aristo->setData(UBGraphicsItemData::ItemLayerType, QVariant(UBItemLayerType::Tool));

    addItem(aristo);

    QPointF itemSceneCenter = aristo->sceneBoundingRect().center();
    aristo->moveBy(center.x() - itemSceneCenter.x(), center.y() - itemSceneCenter.y());

    aristo->setVisible(true);
    setModified(true);
}

void UBGraphicsScene::addCache()
{
    UBGraphicsCache* cache = UBGraphicsCache::instance(this);
    if (!items().contains(cache)) {
        addItem(cache);

        cache->setData(UBGraphicsItemData::ItemLayerType, QVariant(UBItemLayerType::Tool));

        cache->setVisible(true);
        cache->setSelected(true);
        UBApplication::boardController->notifyCache(true);
        UBApplication::boardController->notifyPageChanged();
    }
}

void UBGraphicsScene::addMask(const QPointF &center)
{
    UBGraphicsCurtainItem* curtain = new UBGraphicsCurtainItem(); // mem : owned and destroyed by the scene
    mTools << curtain;

    addItem(curtain);

    QRectF rect = UBApplication::boardController->activeScene()->normalizedSceneRect();
    rect.setRect(center.x() - rect.width()/4, center.y() - rect.height()/4, rect.width()/2 , rect.height()/2);
    curtain->setRect(rect);
    curtain->setVisible(true);
    curtain->setSelected(true);
}

void UBGraphicsScene::setRenderingQuality(UBItem::RenderingQuality pRenderingQuality)
{
    QListIterator<QGraphicsItem*> itItems(mFastAccessItems);

    while (itItems.hasNext())
    {
        QGraphicsItem *gItem =  itItems.next();

        UBItem *ubItem = dynamic_cast<UBItem*>(gItem);

        if (ubItem)
        {
            ubItem->setRenderingQuality(pRenderingQuality);
        }
    }
}

QList<QUrl> UBGraphicsScene::relativeDependencies() const
{
    QList<QUrl> relativePathes;

    QListIterator<QGraphicsItem*> itItems(items());

    while (itItems.hasNext())
    {
        QGraphicsItem* item = itItems.next();
        UBGraphicsMediaItem *videoItem = qgraphicsitem_cast<UBGraphicsMediaItem*> (item);

        if (videoItem)
        {
            QString mediaUrl = videoItem->mediaFileUrl().toLocalFile();
            relativePathes << mediaUrl.replace(QRegExp("\\{.*\\}"), UBGraphicsItem::getOwnUuid(videoItem).toString());
        }

        UBGraphicsItem* ubItem = dynamic_cast<UBGraphicsItem*>(item);
        if(ubItem && ubItem->Delegate()->action() && ubItem->Delegate()->action()->linkType() == eLinkToAudio)
            relativePathes << QUrl(ubItem->Delegate()->action()->path().replace(document()->persistencePath(),""));

    }

    qDebug() << "relative pathes"<< relativePathes;

    return relativePathes;
}

QSize UBGraphicsScene::nominalSize()
{
    if (mDocument && !mNominalSize.isValid())
    {
        mNominalSize = mDocument->defaultDocumentSize();
        emit pageSizeChanged();// Issue 1684 - CFA -20131127
    }

    return mNominalSize;
}

void UBGraphicsScene::setNominalSize(const QSize& pSize)
{
    if (nominalSize() != pSize)
    {
        mNominalSize = pSize;
        emit pageSizeChanged();

        if(mDocument)
            mDocument->setDefaultDocumentSize(pSize);

    }
}

void UBGraphicsScene::setNominalSize(int pWidth, int pHeight)
{
     setNominalSize(QSize(pWidth, pHeight));
}

void UBGraphicsScene::setSelectedZLevel(QGraphicsItem * item)
{
    item->setZValue(mZLayerController->generateZLevel(itemLayerType::SelectedItem));
}

void UBGraphicsScene::setOwnZlevel(QGraphicsItem *item)
{
    item->setZValue(item->data(UBGraphicsItemData::ItemOwnZValue).toReal());
}

QUuid UBGraphicsScene::getPersonalUuid(QGraphicsItem *item)
{
    QString idCandidate = item->data(UBGraphicsItemData::ItemUuid).toString();
    return idCandidate == QUuid().toString() ? QUuid() : QUuid(idCandidate);
}

qreal UBGraphicsScene::changeZLevelTo(QGraphicsItem *item, UBZLayerController::moveDestination dest)
{
    return mZLayerController->changeZLevelTo(item, dest);
}

QGraphicsItem* UBGraphicsScene::rootItem(QGraphicsItem* item) const
{
    QGraphicsItem* root = item;

    while (root->parentItem())
    {
        root = root->parentItem();
    }

    return root;
}

void UBGraphicsScene::drawItems (QPainter * painter, int numItems,
        QGraphicsItem * items[], const QStyleOptionGraphicsItem options[], QWidget * widget)
{
    if (mRenderingContext == NonScreen || mRenderingContext == PdfExport)
    {
        int count = 0;

        QGraphicsItem** itemsFiltered = new QGraphicsItem*[numItems];
        QStyleOptionGraphicsItem *optionsFiltered = new QStyleOptionGraphicsItem[numItems];

        for (int i = 0; i < numItems; i++)
        {
            if (!mTools.contains(rootItem(items[i])))
            {
                bool isPdfItem =  qgraphicsitem_cast<UBGraphicsPDFItem*> (items[i]) != NULL;
                if(!isPdfItem || mRenderingContext == NonScreen)
                {
                    itemsFiltered[count] = items[i];
                    optionsFiltered[count] = options[i];
                    count++;
                }
            }
        }

        QGraphicsScene::drawItems(painter, count, itemsFiltered, optionsFiltered, widget);

        delete[] optionsFiltered;
        delete[] itemsFiltered;

    }
    else if (mRenderingContext == Podcast)
    {
        int count = 0;

        QGraphicsItem** itemsFiltered = new QGraphicsItem*[numItems];
        QStyleOptionGraphicsItem *optionsFiltered = new QStyleOptionGraphicsItem[numItems];

        for (int i = 0; i < numItems; i++)
        {
            bool ok;
            int itemLayerType = items[i]->data(UBGraphicsItemData::ItemLayerType).toInt(&ok);
            if (ok && (itemLayerType >= UBItemLayerType::FixedBackground && itemLayerType <= UBItemLayerType::Tool))
            {
                itemsFiltered[count] = items[i];
                optionsFiltered[count] = options[i];
                count++;
            }
        }

        QGraphicsScene::drawItems(painter, count, itemsFiltered, optionsFiltered, widget);

        delete[] optionsFiltered;
        delete[] itemsFiltered;

    }
    else
    {
        QGraphicsScene::drawItems(painter, numItems, items, options, widget);
    }
}

void UBGraphicsScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    if (mIsDesktopMode)
    {
        QGraphicsScene::drawBackground (painter, rect);
        return;
    }
    bool darkBackground = isDarkBackground ();

    if (darkBackground)
    {
      painter->fillRect (rect, QBrush (QColor (Qt::black)));
    }
    else
    {
      painter->fillRect (rect, QBrush (QColor (Qt::white)));
    }

    if (mZoomFactor > 0.5)
    {
        QColor bgCrossColor;

        if (darkBackground)
            bgCrossColor = UBSettings::crossDarkBackground;
        else
            bgCrossColor = UBSettings::crossLightBackground;
        if (mZoomFactor < 1.0)
        {
            int alpha = 255 * mZoomFactor / 2;
            bgCrossColor.setAlpha (alpha); // fade the crossing on small zooms
        }

        painter->setPen (bgCrossColor);

        if (isCrossedBackground())
        {
            qreal firstY = ((int) (rect.y () / UBSettings::crossSize)) * UBSettings::crossSize;

            for (qreal yPos = firstY; yPos < rect.y () + rect.height (); yPos += UBSettings::crossSize)
            {
                painter->drawLine (rect.x (), yPos, rect.x () + rect.width (), yPos);
            }

            qreal firstX = ((int) (rect.x () / UBSettings::crossSize)) * UBSettings::crossSize;

            for (qreal xPos = firstX; xPos < rect.x () + rect.width (); xPos += UBSettings::crossSize)
            {
                painter->drawLine (xPos, rect.y (), xPos, rect.y () + rect.height ());
            }
        }
    }
}

void UBGraphicsScene::keyReleaseEvent(QKeyEvent * keyEvent)
{

    QList<QGraphicsItem*> si = selectedItems();

    if ((si.size() > 0) && (keyEvent->isAccepted()))
    {
#ifdef Q_OS_MAC
        if (keyEvent->key() == Qt::Key_Backspace)
#else
        if (keyEvent->matches(QKeySequence::Delete))
#endif
        {
            // Issue 1569 - CFA - 20131023 : undo erase multi-selection
            UBApplication::undoStack->beginMacro("remove items");

            foreach(QGraphicsItem* item, si)
            {

                switch (item->type())
                {
                case UBGraphicsWidgetItem::Type:
                    {
                        UBGraphicsW3CWidgetItem *wc3_widget = dynamic_cast<UBGraphicsW3CWidgetItem*>(item);
                        if (0 != wc3_widget)
                        if (!wc3_widget->hasFocus())
                            wc3_widget->remove();
                        break;
                    }
                case UBGraphicsTextItem::Type:
                    {
                        UBGraphicsTextItem *text_item = dynamic_cast<UBGraphicsTextItem*>(item);
                        if (0 != text_item)
                        if (!text_item->hasFocus())
                            text_item->remove();
                        break;
                    }

                default:
                    {
                        UBGraphicsItem *ubgi = dynamic_cast<UBGraphicsItem*>(item);

                        if (0 != ubgi)
                             ubgi->remove();
                        else{
                            UBCoreGraphicsScene::removeItem(item);
                        }
                    }
                }
            }

            UBApplication::undoStack->endMacro();
            // Fin Issue 1569 - CFA - 20131023
        }

        keyEvent->accept();
    }

    QGraphicsScene::keyReleaseEvent(keyEvent);
}

void UBGraphicsScene::setDocumentUpdated()
{
    if (document())
        document()->setMetaData(UBSettings::documentUpdatedAt
                , UBStringUtils::toUtcIsoDateTime(QDateTime::currentDateTime()));
}

void UBGraphicsScene::createEraiser()
{
    mEraser = new QGraphicsEllipseItem(); // mem : owned and destroyed by the scene
    mEraser->setRect(QRect(0, 0, 0, 0));
    mEraser->setVisible(false);

    mEraser->setData(UBGraphicsItemData::ItemLayerType, QVariant(UBItemLayerType::Control));
    mEraser->setData(UBGraphicsItemData::itemLayerType, QVariant(itemLayerType::Eraiser)); //Necessary to set if we want z value to be assigned correctly

    mTools << mEraser;
    addItem(mEraser);

}

void UBGraphicsScene::createPointer()
{
    mPointer = new QGraphicsEllipseItem();  // mem : owned and destroyed by the scene
    mPointer->setRect(QRect(0, 0, 20, 20));
    mPointer->setVisible(false);

    mPointer->setPen(Qt::NoPen);
    mPointer->setBrush(QBrush(QColor(255, 0, 0, 186)));

    mPointer->setData(UBGraphicsItemData::ItemLayerType, QVariant(UBItemLayerType::Tool));
    mPointer->setData(UBGraphicsItemData::itemLayerType, QVariant(itemLayerType::Pointer)); //Necessary to set if we want z value to be assigned correctly

    mTools << mPointer;
    addItem(mPointer);
}

void UBGraphicsScene::setToolCursor(int tool)
{
    if (tool == (int)UBStylusTool::Selector ||
             tool == (int)UBStylusTool::Text ||
                tool == (int)UBStylusTool::Play)
    {
        deselectAllItems();
    }

    if (mCurrentStroke && mCurrentStroke->polygons().empty()){
        delete mCurrentStroke;
        mCurrentStroke = NULL;
    }

}

void UBGraphicsScene::initStroke()
{
    mCurrentStroke = new UBGraphicsStroke();
}
