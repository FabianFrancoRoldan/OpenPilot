TEMPLATE = app
TARGET = nesting
CONFIG += qt warn_on

CONFIG += qt3d_deploy_pkg
include(../../../pkg.pri)

SOURCES = cubeview.cpp main.cpp
HEADERS = cubeview.h
RESOURCES = nesting.qrc

OTHER_FILES += \
    nesting.rc

RC_FILE = nesting.rc

