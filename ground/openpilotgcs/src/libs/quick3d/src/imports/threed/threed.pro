TEMPLATE = lib
TARGET  = qthreedqmlplugin
CONFIG += qt plugin

CONFIG += qt3dquick_deploy_pkg
include(../../../pkg.pri)

# See the README in the root dir re this code
package {
    target.path += $$QT3D_INSTALL_IMPORTS/Qt3D
    INSTALLS += target
} else {
    CONFIG += qt3dquick qt3d
    DESTDIR = $$QT3D_INSTALL_IMPORTS/Qt3D
}

win32 {
    CONFIG(debug, debug|release) {
        TARGET = $$member(TARGET, 0)d
    }
}

symbian {
    CONFIG += epocallowdlldata
    MMP_RULES += EXPORTUNFROZEN
    LIBS += -lQt3D$${QT_LIBINFIX} -lQt3DQuick$${QT_LIBINFIX}
    TARGET.CAPABILITY = All -Tcb
    TARGET.UID3 = 0x20031E9B
}

SOURCES += \
    threed.cpp \
    qt3dnamespace.cpp \
    scale3d.cpp \
    viewport.cpp \
    qgraphicslookattransform.cpp \
    shaderprogram.cpp \
    skybox.cpp \
    billboarditem3d.cpp

HEADERS += \
    qt3dnamespace.h \
    scale3d.h \
    viewport.h \
    qgraphicslookattransform.h \
    shaderprogram.h \
    shaderprogram_p.h \
    skybox.h \
    billboarditem3d.h

# See the file README.library_xml for more on this
qdeclarativesources.files += \
    qmldir \
    library.xml \
    plugins.qmltypes

!symbian {
    !package {
        qdeclarative_in_place.input = qdeclarativesources.files
        qdeclarative_in_place.output = $$QT3D_INSTALL_IMPORTS/Qt3D/${QMAKE_FILE_IN_BASE}${QMAKE_FILE_EXT}
        qdeclarative_in_place.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
        qdeclarative_in_place.CONFIG += no_link_no_clean
        qdeclarative_in_place.variable_out = PRE_TARGETDEPS
        QMAKE_EXTRA_COMPILERS += qdeclarative_in_place
    } else {
        qdeclarativesources.path += $$QT3D_INSTALL_IMPORTS/Qt3D
        INSTALLS += qdeclarativesources
    }
}  else {
    importFiles.sources = $$qdeclarativesources.files
    importFiles.path = $$QT_IMPORTS_BASE_DIR/Qt3D/
    DEPLOYMENT = importFiles
}

OTHER_FILES += \
    README.plugins_types \
    README.library_xml


