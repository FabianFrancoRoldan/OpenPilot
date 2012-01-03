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

import QtQuick 1.0
import Qt3D 1.0
import Qt3D.Shapes 1.0

// QTBUG-17392 / QTBUG-17391
Item {
    id: screen
    width: 720
    height: 480

    //ITEM 1: Points
    Rectangle {
        id: panel1
        x: 0; y:0
        width:  screen.width/3
        height: screen.height/3
        border.width:  3
        color: "#3333aa"

        Viewport  {
            anchors.fill: parent
            camera: Camera { eye: Qt.vector3d(0,3,15)}
            picking: true

            Point {
                vertices: [
                   -1, 1, 0,
                   -1, -2, 0,
                   1, -2, 0
                ]
                pointSize:  3
                onClicked: console.log("Points clicked")
            }
        }

        Rectangle {
            border.width: 3; radius: 5
            width: parent.width
            height: text1.height + 10
            Text {
                id: text1
                width: parent.width
                wrapMode: "WordWrap"
                text: "  POINT CONSTRUCTION:\n  3 Points in an L shape should be visible."
                anchors.centerIn: parent
            }
        }
    }

    //ITEM 2: Lines
    Rectangle {
        id: panel2
        anchors.left: panel1.right
        anchors.top:  parent.top
        width:  screen.width/3
        height: screen.height/3
        border.width:  3
        color: "#3333aa"

        Viewport  {
            anchors.fill: parent
            camera: Camera { eye: Qt.vector3d(0,3,15)}
            picking: true

            Line {
                vertices: [
                   -1, 1, 0,
                   -1, -2, 0,
                   1, -2, 0
                ]
                width:  3
                onClicked: console.log("Line clicked")
            }
        }

        Rectangle {
            border.width: 3; radius: 5
            width: parent.width
            height: text2.height + 10
            Text {
                id: text2
                width: parent.width
                wrapMode: "WordWrap"
                text: "  LINE CONSTRUCTION:\n  2 Lines in an L shape should be visible."
                anchors.centerIn: parent
            }
        }
    }

    //ITEM 3: A Sphere
    Rectangle {
        id: panel3
        anchors.left: panel2.right
        anchors.top:  parent.top
        width:  screen.width/3
        height: screen.height/3
        border.width:  3
        color: "#3333aa"

        Viewport  {
            anchors.fill: parent
            camera: Camera { eye: Qt.vector3d(0,0,10)}
            picking: true

            Sphere {onClicked: console.log("Sphere clicked")}
        }
        //text for item3
        Rectangle {
            border.width: 3; radius: 5
            width: parent.width
            height: text3.height + 10
            Text {
                id: text3
                width: parent.width
                wrapMode: "WordWrap"
                text: "  SPHERE CONSTRUCTION:\n  A shaded sphere should be visible."
                anchors.centerIn: parent
            }
        }
    }

     //ITEM 4: A Cylinder
    Rectangle {
        id: panel4
        anchors.left: parent.left
        anchors.top:  panel1.bottom
        width:  screen.width/3
        height: screen.height/3
        border.width:  3
        color: "#3333aa"

        Viewport  {
            anchors.fill: parent
            camera: Camera { eye: Qt.vector3d(0,0,10)}
            picking: true

            Cylinder {height: 3; onClicked: console.log("Cylinder clicked");}
        }

        Rectangle {
            border.width: 3; radius: 5
            width: parent.width
            height: text4.height + 10
            Text {
                id: text4
                width: parent.width
                wrapMode: "WordWrap"
                text: "  CYLINDER CONSTRUCTION:\n  A shaded cylinder with 8 sides should be visible."
                anchors.centerIn: parent
            }
        }
    }

   //ITEM 5: A Capsule
   Rectangle {
       id: panel5
       anchors.left: panel4.right
       anchors.top:  panel2.bottom
       width:  screen.width/3
       height: screen.height/3
       border.width:  3
       color: "#3333aa"

       Viewport  {
           anchors.fill: parent
           camera: Camera { eye: Qt.vector3d(0,0,10)}
           picking: true

           Capsule {onClicked: console.log("Capsule clicked");}

       }

       Rectangle {
           border.width: 3; radius: 5
           width: parent.width
           height: text5.height + 10
           Text {
               id: text5
               width: parent.width
               wrapMode: "WordWrap"
               text: "  CAPSULE CONSTRUCTION:\n  A shaded capsule with 8 sides should be visible."
               anchors.centerIn: parent
           }
       }
   }

   //ITEM 6: A Quad
   Rectangle {
       id: panel6
       anchors.left: panel5.right
       anchors.top:  panel3.bottom
       width:  screen.width/3
       height: screen.height/3
       border.width:  3
       color: "#3333aa"

       Viewport  {
           anchors.fill: parent
           camera: Camera {eye: Qt.vector3d(0,5,10)}
           picking: true

           Quad {
               scale: 2;
               onClicked: console.log("Quad clicked")
           }
       }

       Rectangle {
           border.width: 3; radius: 5
           width: parent.width
           height: text6.height + 10
           Text {
               id: text6
               width: parent.width
               wrapMode: "WordWrap"
               text: "  .OBJ LOADING:\n  A planar quad should be visible."
               anchors.centerIn: parent
           }
       }
   }

   //ITEM 7: A Cube
  Rectangle {
      id: panel7
      anchors.left: parent.left
      anchors.top:  panel4.bottom
      width:  screen.width/3
      height: screen.height/3
      border.width:  3
      color: "#3333aa"


      Viewport  {
          anchors.fill: parent
          camera: Camera { eye: Qt.vector3d(0,0,10)}
          picking: true

          Cube {
              scale: 2
              onClicked: console.log("Cube clicked");
          }
      }

      Rectangle {
          border.width: 3; radius: 5
          width: parent.width
          height: text7.height + 10
          Text {
              id: text7
              width: parent.width
              wrapMode: "WordWrap"
              text: "  .OBJ LOADING:\n  A shaded cube should be visible."
              anchors.centerIn: parent
          }
      }
  }

 //ITEM 8: A Teapot
 Rectangle {
     id: panel8
     anchors.left: panel7.right
     anchors.top:  panel5.bottom
     width:  screen.width/3
     height: screen.height/3
     border.width:  3
     color: "#3333aa"

     Viewport  {
         anchors.fill: parent
         camera: Camera { eye: Qt.vector3d(0,0,10)}
         picking: true

         Teapot {
             scale: 1.3
             onClicked: console.log("Teapot clicked");
         }
     }

     Rectangle {
         border.width: 3; radius: 5
         width: parent.width
         height: text8.height + 10
         Text {
             id: text8
             width: parent.width
             wrapMode: "WordWrap"
             text: "  .BEZ LOADING:\n  A shaded teapot should be visible."
             anchors.centerIn: parent
         }
     }
 }

 //ITEM 9: A Lunar Lander
 Rectangle {
     id: panel9
     anchors.left: panel8.right
     anchors.top:  panel6.bottom
     width:  screen.width/3
     height: screen.height/3
     border.width:  3
     color: "#3333aa"

     Viewport  {
         anchors.fill: parent
         camera: Camera { eye: Qt.vector3d(0,0,10)}
         picking: true

         Item3D {
             scale: 0.5
             position: Qt.vector3d(0,-1,0)
             mesh: Mesh {source: "meshes/lunar-lander.3ds"}
             onClicked: console.log("Lunar lander clicked")
         }
     }

     Rectangle {
         border.width: 3; radius: 5
         width: parent.width
         height: text9.height + 10
         Text {
             id: text9
             width: parent.width
             wrapMode: "WordWrap"
             text: "  .3DS LOADING:\n  A lunar lander module should be visible."
             anchors.centerIn: parent
         }
     }
 }
}

