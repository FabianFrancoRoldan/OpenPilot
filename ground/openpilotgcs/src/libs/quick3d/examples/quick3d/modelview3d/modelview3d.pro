TEMPLATE = app
TARGET = modelview3d_qml
CONFIG += qt warn_on

INSTALL_DIRS = qml
CONFIG += qt3d_deploy_qml qt3dquick_deploy_pkg
include(../../../pkg.pri)
qtcAddDeployment()

SOURCES += main.cpp

OTHER_FILES += \
    modelview3d_qml.rc

RC_FILE = modelview3d_qml.rc
