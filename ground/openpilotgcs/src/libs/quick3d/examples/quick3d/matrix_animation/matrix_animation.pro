TEMPLATE = app
TARGET = matrix_animation
CONFIG += qt warn_on

INSTALL_DIRS = qml
CONFIG += qt3d_deploy_qml qt3dquick_deploy_pkg
include(../../../pkg.pri)
qtcAddDeployment()

SOURCES += main.cpp

OTHER_FILES += \
    matrix_animation.rc

RC_FILE = matrix_animation.rc
