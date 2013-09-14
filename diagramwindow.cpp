#include <QtWidgets>
#include "diagramwindow.h"
#include "DocumentModel.h"
#include "ILabelizable.h"
#include "IPositionable.h"

int DiagramWindow::theGeneration = 1;

DiagramWindow::DiagramWindow(QMenu *theObjectMenu)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    // Maybe later :P [auto-pan]
    setDragMode(RubberBandDrag);

    theScene = new DiagramScene(theObjectMenu, this);
    theScene->setSceneRect(QRectF(0, 0, 5000, 5000));

    setScene(theScene);
    setSceneRect(theScene->sceneRect());

    theModel.reset(new DocumentModel(tr("Untitled %1").arg(DiagramWindow::theGeneration).toStdString(), std::string("A use-case diagram")));

    connect(theScene, SIGNAL(itemInserted(const INode*)), this, SLOT(nodeAdded(const INode*)));
    connect(theScene, SIGNAL(relationEstablished(uint16_t,uint16_t,const IRelation*)), this, SLOT(relationEstablished(uint16_t,uint16_t,const IRelation*)));
    connect(theScene, SIGNAL(changeNodeName(uint16_t,QString)), this, SLOT(nodeNameChanged(uint16_t,QString)));
    connect(theScene, SIGNAL(changeLabelPosition(uint16_t,QPointF)), this, SLOT(labelPositionChanged(uint16_t,QPointF)));
    connect(theScene, SIGNAL(itemsMayHaveMoved()), this, SLOT(nodePositionsMayHaveChanged()));

    theZoomLevel = 1;

    ++theGeneration;
}

void DiagramWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
}

void DiagramWindow::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

        double scaleFactor = 1.15;
        if(event->delta() > 0) {
            // Zoom in
            scale(scaleFactor, scaleFactor);
            theZoomLevel *= 1.15;
        } else {
            // Zooming out
            scale(1.0 / scaleFactor, 1.0 / scaleFactor);
            theZoomLevel /= scaleFactor;
        }
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void DiagramWindow::zoomIn()
{
    double scaleFactor = 1.15;
    scale(scaleFactor, scaleFactor);

    theZoomLevel *= 1.15;
}

void DiagramWindow::zoomOut()
{
    double scaleFactor = 1.15;
    scale(1.0 / scaleFactor, 1.0 / scaleFactor);
    theZoomLevel /= 1.15;
}

void DiagramWindow::fitWindow()
{
    QList<QGraphicsItem *> items = theScene->items();
    QRectF box;

    foreach (QGraphicsItem *item, items) {
        box |= item->boundingRect();
    }

    if (box.width() == 0) {
        box = sceneRect();
    }

    fitInView(box, Qt::KeepAspectRatio);
}

void DiagramWindow::resetZoom()
{
    double compensatingFactor = 1 / theZoomLevel;
    theZoomLevel = 1;

    scale(compensatingFactor, compensatingFactor);
}

void DiagramWindow::deleteSelectedItem()
{
    QList<QGraphicsItem *> items = theScene->selectedItems();

    foreach (QGraphicsItem *item, items) {
         if (Arrow *arrow = dynamic_cast<Arrow *>(item)) {
            const INode *thisNode = theModel->getNode(arrow->startItem()->getId()),
                        *thatNode = theModel->getNode(arrow->endItem()->getId());

            if (thisNode && thatNode) {
                const IRelation *theRelation = theModel->getRelation(thisNode, thatNode);

                if (theRelation) {
                    theModel->breakUp(theRelation);
                    theScene->removeItem(item);
                }
            }
        }
    }

    items.clear();

    items = theScene->selectedItems();

    foreach (QGraphicsItem *item, items) {
        if (DiagramItem *it = dynamic_cast<DiagramItem *>(item)) {
            const INode *theNode = theModel->getNode(it->getId());

            theModel->removeNode(theNode);
            theScene->removeItem(item);
        }
    }
}

void DiagramWindow::documentWasModified()
{

}

QString DiagramWindow::getTitle() const
{
    return QString::fromStdString(theModel.get()->getTitle());
}

void DiagramWindow::setItemType(int theItemType)
{
    theScene->setItemType(static_cast<NodeType>(theItemType));
}

void DiagramWindow::setLineType(int theLineType)
{
    theScene->setLineType(static_cast<RelationType>(theLineType));
}

void DiagramWindow::setMode(int theMode)
{
    theScene->setMode(static_cast<DiagramScene::Mode>(theMode));
}

void DiagramWindow::nodeAdded(const INode *theNode)
{
    theModel->addNode(theNode);

    emit nodeInserted();
}

void DiagramWindow::relationEstablished(const uint16_t thisNodeId, const uint16_t thatNodeId, const IRelation *theRelation)
{
    const INode *thisNode = theModel->getNode(thisNodeId),
                *thatNode = theModel->getNode(thatNodeId);

    theModel->relate(theRelation, thisNode, thatNode);
}

void DiagramWindow::nodeNameChanged(const uint16_t theNodeId, const QString &theText)
{
    INode *theNode = theModel->grabNode(theNodeId);

    if (ILabelizable *theLabel = dynamic_cast<ILabelizable *>(theNode)) {
        theLabel->setName(theText.toStdString());
    }
}

void DiagramWindow::labelPositionChanged(const uint16_t theNodeId, const QPointF &thePosition)
{
    INode *theNode = theModel->grabNode(theNodeId);

    if (ILabelizable *theLabel = dynamic_cast<ILabelizable *>(theNode)) {
        // Sets relative position of label to origin of the parent item
        // here, that would be the node
        theLabel->setLabelPosition(thePosition.x(), thePosition.y());
    }
}

void DiagramWindow::nodePositionsMayHaveChanged()
{
    QList<QGraphicsItem *> items = theScene->selectedItems();

    foreach (QGraphicsItem *anItem, items) {
        if (DiagramItem *thisItem = dynamic_cast<DiagramItem *>(anItem)) {
            INode *theNode = theModel->grabNode(thisItem->getId());

            if (IPositionable *thePositioner = dynamic_cast<IPositionable *>(theNode)) {
                thePositioner->setPosition(thisItem->x(), thisItem->y(), thisItem->zValue());
            }
        }
    }
}
