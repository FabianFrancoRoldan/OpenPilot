/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtQuick3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

import Qt 4.7
import Qt3D 1.0
import Qt3D.Shapes 1.0
import QtQuickTest 1.0

Rectangle
{
    id: topLevel
    width: 480; height: 480

    Viewport {
        id: viewport
        picking: true
        anchors.fill: parent

        camera: Camera {
            id: main_camera
            eye: Qt.vector3d(0, 4, 12)
            center: Qt.vector3d(0, 0, -2.5)
        }

        property variant emptyStringListModel:[]

        property variant stringListModelForViewport:[
            "textures/threedgreen.jpg",
            "textures/amethyst.jpg",
            "textures/bigblue.jpg",
            "textures/pastelstuff.jpg",
            "textures/blueweb.jpg",
            "textures/qtlogo.png"
        ]

        property variant stringListModelEmptyForItem: []

        property variant stringListModelForItem:[
            "textures/amethyst.jpg",
            "textures/pastelstuff.jpg",
            "textures/bigblue.jpg",
            "textures/qtlogo.png",
            "textures/blueweb.jpg",
            "textures/threedgreen.jpg"
        ]

        ListModel {
            id: listModelForViewport
            ListElement { image:"textures/threedgreen.jpg"}
            ListElement { image:"textures/amethyst.jpg"}
            ListElement { image:"textures/bigblue.jpg"}
            ListElement { image:"textures/pastelstuff.jpg"}
            ListElement { image:"textures/blueweb.jpg"}
            ListElement { image:"textures/qtlogo.png"}
        }

        ListModel {
            id: listModelForItem
            ListElement { image:"textures/amethyst.jpg"}
            ListElement { image:"textures/pastelstuff.jpg"}
            ListElement { image:"textures/bigblue.jpg"}
            ListElement { image:"textures/qtlogo.png"}
            ListElement { image:"textures/blueweb.jpg"}
            ListElement { image:"textures/threedgreen.jpg"}
        }

        Component {
            id: paneComponentstringList
            Quad {
                property real layer: index
                property variant image : modelData
                position: Qt.vector3d(0, 0, -layer)
                effect: Effect { decal: true; texture: image }
                transform: Rotation3D { axis: Qt.vector3d(1,0,0); angle: 90 }
            }
        }

        Component {
            id: paneComponentListModel
            Quad {
                position: Qt.vector3d(index * 0.1, 0, -index)
                enabled: index != -1
                effect: Effect { decal: true;
                    texture: model.image;
                }
                transform: Rotation3D { axis: Qt.vector3d(1,0,0); angle: 90 }
            }
        }

        Repeater {
            id: stringListModelRepeaterInViewport
            delegate: paneComponentstringList
            model: parent.stringListModelEmpty
        }

        Repeater {
            id: listModelRepeaterInViewport
            delegate: paneComponentListModel
            model:  listModelForViewport
        }

        Item3D {
            id: stringListModelParentItem
            x: -2.1
            Repeater {
                id: stringListModelRepeaterInItem
                delegate: paneComponentstringList
                model: viewport.stringListModelEmpty
            }
        }

        Item3D {
            id: listModelParentItem
            x: 2.1
            y: 0.5

            Repeater {
                id: listModelRepeaterInItem
                delegate: paneComponentListModel
                model:  listModelForItem
            }
        }

        TestCase {
            id: modelViewTestAddingCase
//            when: false
            name: "Quick3d ModelView Adding Data Test"

            function test_changing_stringList_model() {
                var viewportEmptyModelChildCount = viewport.children.length;
                var itemEmptyModelChildCount =
                    stringListModelParentItem.children.length;
                var modelLength = viewport.stringListModelForViewport.length;

                stringListModelRepeaterInViewport.model =
                        viewport.stringListModelForViewport;

                verify(viewport.children.length >
                       viewportEmptyModelChildCount,
                       "Children not added to viewport with changed stringList model");
                compare(viewport.children.length,
                        viewportEmptyModelChildCount +
                        viewport.stringListModelForViewport.length,
                        "Viewport has unexpected number of new children");

                stringListModelRepeaterInItem.model =
                        viewport.stringListModelForItem;

                verify(stringListModelParentItem.children.length >
                       itemEmptyModelChildCount,
                       "Children not added to Item3D with changed stringList model");
                compare(stringListModelParentItem.children.length,
                        itemEmptyModelChildCount +
                        viewport.stringListModelForItem.length,
                        "Item3D has unexpected number of new children");
            }

            function test_adding_to_listModel() {
                var viewportChildCount = viewport.children.length;
                var itemChildCount = listModelParentItem.children.length;

                listModelForViewport.append({"image":"textures/pastelstuff.jpg"});
                listModelForItem.append({"image":"textures/pastelstuff.jpg"});
                viewport.update3d();
                compare(viewport.children.length, viewportChildCount + 1,
                        "Viewport missing child after listModel.append()");
                compare(listModelParentItem.children.length,
                        itemChildCount + 1,
                        "Item3D missing child after listModel.append()");
            }
        }

        // This timer is to avoid an issue where removing children is
        // delayed during initialization.
        Timer {
            running: true
            interval: 1
            onTriggered: modelViewRemovingItemsTestCase.when = true;
        }

        TestCase {
            id: modelViewRemovingItemsTestCase
            // Wait for event loop before performing removal tests
            when: false
            name: "Quick3d ModelView Removing Data Test"

            function test_removing_from_listModel() {
                var viewportChildCount = viewport.children.length;
                var itemChildCount = listModelParentItem.children.length;

                listModelForViewport.remove(listModelForViewport.count - 1);
                compare(viewport.children.length, viewportChildCount -1,
                        "Viewport has extra children after listModel.remove()")

                listModelForItem.remove(listModelForItem.count -1);
                compare(listModelParentItem.children.length,
                        itemChildCount - 1,
                        "Item3D has extra children after listModel.remove()");
            }

            function test_clearing_stringList() {
                stringListModelRepeaterInViewport.model =
                        viewport.stringListModelForViewport;
                stringListModelRepeaterInItem.model =
                        viewport.stringListModelForItem;

                var viewportChildCount = viewport.children.length;
                var itemChildCount = stringListModelParentItem.children.length;

                stringListModelRepeaterInViewport.model =
                        viewport.emptyStringListModel;
                stringListModelRepeaterInItem.model =
                        viewport.emptyStringListModel;

                verify(viewport.children.length < viewportChildCount,
                       "Children not removed from viewport when clearing stringList model");
                compare(viewport.children.length,
                        viewportChildCount -
                            viewport.stringListModelForViewport.length,
                        "Unexpected number of children after setting empty model");

                verify(stringListModelParentItem.children.length <
                        itemChildCount,
                       "Children not removed from Item3D when clearing stringList model");
                compare(stringListModelParentItem.children.length,
                        itemChildCount -
                            viewport.stringListModelForItem.length,
                        "Unexpected number of children after setting empty model");
            }
        }
    }
}
