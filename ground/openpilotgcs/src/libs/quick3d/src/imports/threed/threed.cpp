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

#include <QtDeclarative/qdeclarativeextensionplugin.h>
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecontext.h>
#include <QtDeclarative/qdeclarative.h>

#include "qdeclarativeitem3d.h"
#include "qdeclarativemesh.h"
#include "viewport.h"
#include "qdeclarativeeffect.h"
#include "scale3d.h"
#include "skybox.h"

#include "qgraphicsrotation3d.h"
#include "qgraphicstranslation3d.h"
#include "qgraphicsscale3d.h"
#include "qgraphicsbillboardtransform.h"
#include "qglscenenode.h"

#include "qgraphicslookattransform.h"
#include "shaderprogram.h"
#include "qt3dnamespace.h"

#include "billboarditem3d.h"

QT_BEGIN_NAMESPACE

QML_DECLARE_TYPE(QGraphicsTransform3D)
QML_DECLARE_TYPE(QGraphicsRotation3D)
QML_DECLARE_TYPE(QGraphicsTranslation3D)
QML_DECLARE_TYPE(QGraphicsScale3D)
QML_DECLARE_TYPE(QGraphicsBillboardTransform)
QML_DECLARE_TYPE(QGraphicsLookAtTransform)
QML_DECLARE_TYPE(QGLMaterial)
QML_DECLARE_TYPE(QGLLightModel)
QML_DECLARE_TYPE(QGLLightParameters)
QML_DECLARE_TYPE(QGLCamera)

class QThreedQmlModule : public QDeclarativeExtensionPlugin
{
    Q_OBJECT
public:
    virtual void registerTypes(const char *uri)
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("Qt3D"));
        qmlRegisterType<QGLSceneNode>(uri,1,0,"SceneNode");
        qmlRegisterType<QDeclarativeEffect>(uri,1,0,"Effect");
        qmlRegisterType<QDeclarativeMesh>(uri,1,0,"Mesh");
        qmlRegisterType<QDeclarativeItem3D>(uri,1,0,"Item3D");
        qmlRegisterType<QGLLightModel>(uri,1,0,"LightModel");
        qmlRegisterType<QGLLightParameters>(uri,1,0,"Light");
        qmlRegisterType<QGLCamera>(uri,1,0,"Camera");
        qmlRegisterType<QGraphicsRotation3D>(uri,1,0,"Rotation3D");
        qmlRegisterType<QGraphicsTranslation3D>(uri,1,0,"Translation3D");
        qmlRegisterType<Scale3D>(uri,1,0,"Scale3D");
        qmlRegisterType<QGraphicsBillboardTransform>(uri,1,0,"BillboardTransform");
        qmlRegisterType<QGraphicsLookAtTransform>(uri,1,0,"LookAt");
        qmlRegisterType<QGLMaterial>(uri,1,0,"Material");
        qmlRegisterType<ShaderProgram>(uri,1,0,"ShaderProgram");
        qmlRegisterType<Skybox>(uri, 1, 0, "Skybox");
        qmlRegisterType<BillboardItem3D>(uri, 1, 0, "BillboardItem3D");

        qmlRegisterType<Viewport>(uri,1,0,"Viewport");

        // Needed to make QDeclarativeListProperty<QGraphicsTransform3D> work.
        qmlRegisterType<QGraphicsTransform3D>();
        qmlRegisterType<QGraphicsScale3D>();

#ifdef QT_USE_SCENEGRAPH
        qmlRegisterType<ViewportSG>(uri,2,0,"Viewport");
#endif
    }
    void initializeEngine(QDeclarativeEngine *engine, const char *uri)
    {
        Q_UNUSED(uri);
        QDeclarativeContext *context = engine->rootContext();
        context->setContextProperty(QLatin1String("Qt3D"), new Qt3DNamespace);
    }
};

QT_END_NAMESPACE

#include "threed.moc"

Q_EXPORT_PLUGIN2(qthreedqmlplugin, QT_PREPEND_NAMESPACE(QThreedQmlModule));
