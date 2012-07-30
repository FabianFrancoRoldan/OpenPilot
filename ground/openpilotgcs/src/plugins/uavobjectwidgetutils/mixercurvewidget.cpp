/**
 ******************************************************************************
 *
 * @file       mixercurvewidget.cpp
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @addtogroup GCSPlugins GCS Plugins
 * @{
 * @addtogroup UAVObjectWidgetUtils Plugin
 * @{
 * @brief Utility plugin for UAVObject to Widget relation management
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "mixercurvewidget.h"
#include "mixercurveline.h"
#include "mixercurvepoint.h"

#include <QtGui>
#include <QDebug>

/*
 * Initialize the widget
 */
MixerCurveWidget::MixerCurveWidget(QWidget *parent) : QGraphicsView(parent)
{

    // Create a layout, add a QGraphicsView and put the SVG inside.
    // The Mixer Curve widget looks like this:
    // |--------------------|
    // |                    |
    // |                    |
    // |     Graph  |
    // |                    |
    // |                    |
    // |                    |
    // |--------------------|


    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setRenderHint(QPainter::Antialiasing);

    curveMin=0.0;
    curveMax=1.0;


    setFrameStyle(QFrame::NoFrame);
    setStyleSheet("background:transparent");

    QGraphicsScene *scene = new QGraphicsScene(this);
    QSvgRenderer *renderer = new QSvgRenderer();
    plot = new QGraphicsSvgItem();
    renderer->load(QString(":/configgadget/images/curve-bg.svg"));
    plot->setSharedRenderer(renderer);
    //plot->setElementId("map");
    scene->addItem(plot);
    plot->setZValue(-1);

    scene->setSceneRect(plot->boundingRect());
    setScene(scene);

    initNodes(MixerCurveWidget::NODE_NUMELEM);
}

MixerCurveWidget::~MixerCurveWidget()
{
    while (!nodePool.isEmpty())
        delete nodePool.takeFirst();

    while (!edgePool.isEmpty())
        delete edgePool.takeFirst();
}

Node* MixerCurveWidget::getNode(int index)
{
    Node* node;

    if (index >= 0 && index < nodePool.count())
    {
        node = nodePool.at(index);
    }
    else {
        node = new Node(this);
        nodePool.append(node);
    }
    return node;
}

Edge* MixerCurveWidget::getEdge(int index, Node* sourceNode, Node* destNode)
{
    Edge* edge;

    if (index >= 0 && index < edgePool.count())
    {
        edge = edgePool.at(index);
        edge->setSourceNode(sourceNode);
        edge->setDestNode(destNode);
    }
    else {
        edge = new Edge(sourceNode,destNode);
        edgePool.append(edge);
    }
    return edge;
}

/**
  Init curve: create a (flat) curve with a specified number of points.

  If a curve exists already, resets it.
  Points should be between 0 and 1.
  */
void MixerCurveWidget::initCurve(const QList<double>* points)
{
    if (points->length() < 2)
        return; // We need at least 2 points on a curve!

    // finally, set node positions
    setCurve(points);
}

void MixerCurveWidget::initNodes(int numPoints)
{
    // First of all, clear any existing list
    if (nodeList.count()) {
        foreach (Node *node, nodeList ) {
            foreach(Edge *edge, node->edges()) {
                if (edge->sourceNode() == node) {
                    scene()->removeItem(edge);
                }
            }
            scene()->removeItem(node);
        }

        nodeList.clear();
    }

    // Create the nodes and edges
    Node* prevNode = 0;
    for (int i=0; i<numPoints; i++) {

        Node *node = getNode(i);

        nodeList.append(node);
        scene()->addItem(node);

        node->setPos(0,0);

        if (prevNode) {
            scene()->addItem(getEdge(i, prevNode, node));
        }

        prevNode = node;
    }
}

/**
  Returns the current curve settings
  */
QList<double> MixerCurveWidget::getCurve() {

    QList<double> list;

    foreach(Node *node, nodeList) {
        list.append(node->value());
    }

    return list;
}
/**
  Sets a linear graph
  */
void MixerCurveWidget::initLinearCurve(int numPoints, double maxValue, double minValue)
{
    double range = setRange(minValue, maxValue);

    QList<double> points;
    for (double i=0; i < (double)numPoints; i++) {
        double val = (range * ( i / (double)(numPoints-1) ) ) + minValue;
        points.append(val);
    }
    initCurve(&points);
}
/**
  Setd the current curve settings
  */
void MixerCurveWidget::setCurve(const QList<double>* points)
{
    curveUpdating = true;

    int ptCnt = points->count();
    if (nodeList.count() != ptCnt)
        initNodes(ptCnt);

    double range = curveMax - curveMin;

    qreal w = plot->boundingRect().width()/(ptCnt-1);
    qreal h = plot->boundingRect().height();
    for (int i=0; i<ptCnt; i++) {

        double val = (points->at(i) < curveMin) ? curveMin : (points->at(i) > curveMax) ? curveMax : points->at(i);

        val += range;
        val -= (curveMin + range);
        val /= range;

        Node* node = nodeList.at(i);
        node->setPos(w*i, h - (val*h));
        node->verticalMove(true);
    }
    curveUpdating = false;

    update();

    emit curveUpdated(points, (double)0);
}


void MixerCurveWidget::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)
    // Thit fitInView method should only be called now, once the
    // widget is shown, otherwise it cannot compute its values and
    // the result is usually a ahrsbargraph that is way too small.
    fitInView(plot, Qt::KeepAspectRatio);

}

void MixerCurveWidget::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event);
    fitInView(plot, Qt::KeepAspectRatio);
}

void MixerCurveWidget::itemMoved(double itemValue)
{
    if (!curveUpdating) {
        QList<double> curve = getCurve();
        emit curveUpdated(&curve, itemValue);
    }
}

void MixerCurveWidget::setMin(double value)
{
    curveMin = value;
}
void MixerCurveWidget::setMax(double value)
{
    curveMax = value;
}
double MixerCurveWidget::getMin()
{
    return curveMin;
}
double MixerCurveWidget::getMax()
{
    return curveMax;
}
double MixerCurveWidget::setRange(double min, double max)
{
    curveMin = min;
    curveMax = max;
    return curveMax - curveMin;
}
