/**
******************************************************************************
*
* @file       waypointitem.cpp
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      A graphicsItem representing a WayPoint
* @see        The GNU Public License (GPL) Version 3
* @defgroup   OPMapWidget
* @{
*
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
#include "waypointitem.h"
#include "homeitem.h"

namespace mapcontrol
{
WayPointItem::WayPointItem(const internals::PointLatLng &coord,int const& altitude, MapGraphicItem *map,wptype type):coord(coord),reached(false),description(""),shownumber(true),isDragging(false),altitude(altitude),map(map),myType(type)
    {
        text=0;
        numberI=0;
        picture.load(QString::fromUtf8(":/markers/images/marker.png"));
        number=WayPointItem::snumber;
        ++WayPointItem::snumber;
        this->setFlag(QGraphicsItem::ItemIsMovable,true);
        this->setFlag(QGraphicsItem::ItemIgnoresTransformations,true);
        this->setFlag(QGraphicsItem::ItemIsSelectable,true);
        SetShowNumber(shownumber);
        RefreshToolTip();
        RefreshPos();

        myHome=NULL;
        QList<QGraphicsItem *> list=map->childItems();
        foreach(QGraphicsItem * obj,list)
        {
            HomeItem* h=qgraphicsitem_cast <HomeItem*>(obj);
            if(h)
                myHome=h;
        }

        if(myHome)
            map->Projection()->offSetFromLatLngs(myHome->Coord(),coord,relativeCoord.distance,relativeCoord.bearing);
        qDebug()<<"RELATIVE DISTANCE SET ON CTOR1"<<relativeCoord.distance;
        connect(myHome,SIGNAL(homePositionChanged(internals::PointLatLng)),this,SLOT(onHomePositionChanged(internals::PointLatLng)));
    connect(this,SIGNAL(waypointdoubleclick(WayPointItem*)),map,SIGNAL(wpdoubleclicked(WayPointItem*)));
    }
    WayPointItem::WayPointItem(const internals::PointLatLng &coord,int const& altitude, const QString &description, MapGraphicItem *map,wptype type):coord(coord),reached(false),description(description),shownumber(true),isDragging(false),altitude(altitude),map(map),myType(type)
    {
        text=0;
        numberI=0;
        picture.load(QString::fromUtf8(":/markers/images/marker.png"));
        number=WayPointItem::snumber;
        ++WayPointItem::snumber;
        this->setFlag(QGraphicsItem::ItemIsMovable,true);
        this->setFlag(QGraphicsItem::ItemIgnoresTransformations,true);
        this->setFlag(QGraphicsItem::ItemIsSelectable,true);
        SetShowNumber(shownumber);
        RefreshToolTip();
        RefreshPos();
        myHome=NULL;
        QList<QGraphicsItem *> list=map->childItems();
        foreach(QGraphicsItem * obj,list)
        {
            HomeItem* h=qgraphicsitem_cast <HomeItem*>(obj);
            if(h)
                myHome=h;
        }
        if(myHome)
        {
            map->Projection()->offSetFromLatLngs(myHome->Coord(),coord,relativeCoord.distance,relativeCoord.bearing);
            qDebug()<<"RELATIVE DISTANCE SET ON CTOR2"<<relativeCoord.distance;
            connect(myHome,SIGNAL(homePositionChanged(internals::PointLatLng)),this,SLOT(onHomePositionChanged(internals::PointLatLng)));
        }
        connect(this,SIGNAL(waypointdoubleclick(WayPointItem*)),map,SIGNAL(wpdoubleclicked(WayPointItem*)));
    }

    WayPointItem::WayPointItem(const distBearing &relativeCoordenate, const int &altitude, const QString &description, MapGraphicItem *map):relativeCoord(relativeCoordenate),reached(false),description(description),shownumber(true),isDragging(false),altitude(altitude),map(map)
    {
        qDebug()<<"RELATIVE DISTANCE SET ON CTOR3"<<relativeCoord.distance;
        myHome=NULL;
        QList<QGraphicsItem *> list=map->childItems();
        foreach(QGraphicsItem * obj,list)
        {
            HomeItem* h=qgraphicsitem_cast <HomeItem*>(obj);
            if(h)
               myHome=h;
        }
        if(myHome)
        {
            connect(myHome,SIGNAL(homePositionChanged(internals::PointLatLng)),this,SLOT(onHomePositionChanged(internals::PointLatLng)));
            coord=map->Projection()->translate(myHome->Coord(),relativeCoord.distance,relativeCoord.bearing);
        }
        myType=relative;
        text=0;
        numberI=0;
        picture.load(QString::fromUtf8(":/markers/images/marker.png"));
        number=WayPointItem::snumber;
        ++WayPointItem::snumber;
        this->setFlag(QGraphicsItem::ItemIsMovable,true);
        this->setFlag(QGraphicsItem::ItemIgnoresTransformations,true);
        this->setFlag(QGraphicsItem::ItemIsSelectable,true);
        SetShowNumber(shownumber);
        RefreshToolTip();
        RefreshPos();
        connect(this,SIGNAL(waypointdoubleclick(WayPointItem*)),map,SIGNAL(wpdoubleclicked(WayPointItem*)));

    }

    void WayPointItem::setWPType(wptype type)
    {
        myType=type;
        emit WPValuesChanged(this);
        RefreshPos();
        RefreshToolTip();
        this->update();
    }

    QRectF WayPointItem::boundingRect() const
    {
        return QRectF(-picture.width()/2,-picture.height(),picture.width(),picture.height());
    }
    void WayPointItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
    {
        Q_UNUSED(option);
        Q_UNUSED(widget);
        painter->drawPixmap(-picture.width()/2,-picture.height(),picture);
        if(this->isSelected())
            painter->drawRect(QRectF(-picture.width()/2,-picture.height(),picture.width()-1,picture.height()-1));
    }
    void WayPointItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
    {
        if(event->button()==Qt::LeftButton)
        {
            emit waypointdoubleclick(this);
        }
    }

    void WayPointItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
    {
        if(event->button()==Qt::LeftButton)
        {
        text=new QGraphicsSimpleTextItem(this);
            textBG=new QGraphicsRectItem(this);

        textBG->setBrush(Qt::yellow);

        text->setPen(QPen(Qt::red));
        text->setPos(10,-picture.height());
        textBG->setPos(10,-picture.height());
        text->setZValue(3);
        RefreshToolTip();
        isDragging=true;
    }
        QGraphicsItem::mousePressEvent(event);
    }
    void WayPointItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
    {
        if(event->button()==Qt::LeftButton)
        {
            if(text)
            {
                delete text;
                text=NULL;
            }
            if(textBG)
            {
                delete textBG;
                textBG=NULL;
            }

            isDragging=false;
            RefreshToolTip();

            emit WPValuesChanged(this);
        }
        QGraphicsItem::mouseReleaseEvent(event);
    }
    void WayPointItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
    {

        if(isDragging)
        {
            coord=map->FromLocalToLatLng(this->pos().x(),this->pos().y());
            QString coord_str = " " + QString::number(coord.Lat(), 'f', 6) + "   " + QString::number(coord.Lng(), 'f', 6);
            if(myHome)
            {
                map->Projection()->offSetFromLatLngs(myHome->Coord(),coord,relativeCoord.distance,relativeCoord.bearing);
                qDebug()<<"RELATIVE DISTANCE SET ON MOUSEMOVEEVENT"<<relativeCoord.distance;
            }
            QString relativeCoord_str = QString::number(relativeCoord.distance) + "m " + QString::number(relativeCoord.bearing*180/M_PI)+"deg";
            text->setText(coord_str+"\n"+relativeCoord_str);
            textBG->setRect(text->boundingRect());

            emit WPValuesChanged(this);
        }
            QGraphicsItem::mouseMoveEvent(event);
    }
    void WayPointItem::SetAltitude(const int &value)
    {
        altitude=value;
        RefreshToolTip();

        emit WPValuesChanged(this);
        this->update();
    }

    void WayPointItem::setRelativeCoord(distBearing value)
    {
        relativeCoord=value;
        if(myHome)
        {
               coord=map->Projection()->translate(myHome->Coord(),relativeCoord.distance,relativeCoord.bearing);
        }
        RefreshPos();
        RefreshToolTip();
        this->update();
    }
    void WayPointItem::SetCoord(const internals::PointLatLng &value)
    {
        coord=value;
        emit WPValuesChanged(this);
        RefreshPos();
        RefreshToolTip();
        this->update();
    }
    void WayPointItem::SetDescription(const QString &value)
    {
        description=value;
        RefreshToolTip();
        emit WPValuesChanged(this);
        this->update();
    }
    void WayPointItem::SetNumber(const int &value)
    {
        emit WPNumberChanged(number,value,this);
        number=value;
        RefreshToolTip();
        numberI->setText(QString::number(number));
        numberIBG->setRect(numberI->boundingRect().adjusted(-2,0,1,0));
        this->update();
    }
    void WayPointItem::SetReached(const bool &value)
    {
        reached=value;
        emit WPValuesChanged(this);
        if(value)
            picture.load(QString::fromUtf8(":/markers/images/bigMarkerGreen.png"));
        else
            picture.load(QString::fromUtf8(":/markers/images/marker.png"));
        this->update();

    }
    void WayPointItem::SetShowNumber(const bool &value)
    {
        shownumber=value;
        if((numberI==0) && value)
        {
            numberI=new QGraphicsSimpleTextItem(this);
            numberIBG=new QGraphicsRectItem(this);
            numberIBG->setBrush(Qt::white);
            numberIBG->setOpacity(0.5);
            numberI->setZValue(3);
            numberI->setPen(QPen(Qt::blue));
            numberI->setPos(0,-13-picture.height());
            numberIBG->setPos(0,-13-picture.height());
            numberI->setText(QString::number(number));
            numberIBG->setRect(numberI->boundingRect().adjusted(-2,0,1,0));
        }
        else if (!value && numberI)
        {
            delete numberI;
            delete numberIBG;
        }
        this->update();
    }
    void WayPointItem::WPDeleted(const int &onumber)
    {
        if(number>onumber) --number;
        numberI->setText(QString::number(number));
        numberIBG->setRect(numberI->boundingRect().adjusted(-2,0,1,0));
        RefreshToolTip();
        this->update();
    }
    void WayPointItem::WPInserted(const int &onumber, WayPointItem *waypoint)
    {
        if(waypoint!=this)
        {
            if(onumber<=number) ++number;
            numberI->setText(QString::number(number));
            RefreshToolTip();
            this->update();
        }
    }

    void WayPointItem::onHomePositionChanged(internals::PointLatLng homepos)
    {
        if(myType==relative)
        {
            coord=map->Projection()->translate(homepos,relativeCoord.distance,relativeCoord.bearing);
            emit WPValuesChanged(this);
            RefreshPos();
            RefreshToolTip();
            this->update();
        }
    }
    void WayPointItem::WPRenumbered(const int &oldnumber, const int &newnumber, WayPointItem *waypoint)
    {
        if (waypoint!=this)
        {
            if(((oldnumber>number) && (newnumber<=number)))
            {
                ++number;
                numberI->setText(QString::number(number));
                numberIBG->setRect(numberI->boundingRect().adjusted(-2,0,1,0));
                RefreshToolTip();
            }
            else if (((oldnumber<number) && (newnumber>number)))
            {
                --number;
                numberI->setText(QString::number(number));
                numberIBG->setRect(numberI->boundingRect().adjusted(-2,0,1,0));
                RefreshToolTip();
            }
            else if (newnumber==number)
            {
                ++number;
                numberI->setText(QString::number(number));
                RefreshToolTip();
            }
            this->update();
        }
    }
    int WayPointItem::type() const
    {
        // Enable the use of qgraphicsitem_cast with this item.
        return Type;
    }

    WayPointItem::~WayPointItem()
    {
        --WayPointItem::snumber;
    }
    void WayPointItem::RefreshPos()
    {
        core::Point point=map->FromLatLngToLocal(coord);
        this->setPos(point.X(),point.Y());
    }
    void WayPointItem::RefreshToolTip()
    {
        QString type_str;
        if(myType==relative)
            type_str="Relative";
        else
            type_str="Absolute";
        QString coord_str = " " + QString::number(coord.Lat(), 'f', 6) + "   " + QString::number(coord.Lng(), 'f', 6);
        QString relativeCoord_str = " Distance:" + QString::number(relativeCoord.distance) + " Bearing:" + QString::number(relativeCoord.bearing*180/M_PI);
        setToolTip(QString("WayPoint Number:%1\nDescription:%2\nCoordinate:%4\nFrom Home:%5\nAltitude:%6\nType:%7").arg(QString::number(WayPointItem::number)).arg(description).arg(coord_str).arg(relativeCoord_str).arg(QString::number(altitude)).arg(type_str));
    }

    int WayPointItem::snumber=0;
}
