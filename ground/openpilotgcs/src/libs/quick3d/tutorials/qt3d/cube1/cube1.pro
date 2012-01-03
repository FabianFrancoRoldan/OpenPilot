TEMPLATE = app
TARGET = cube1
CONFIG += qt warn_on qt3d
SOURCES = cubeview.cpp main.cpp
HEADERS = cubeview.h
DESTDIR = ../../../bin/qt3d/tutorials

symbian {
    vendorinfo = \
     "%{\"Nokia\"}" \
     ":\"Nokia\""

    my_deployment.pkg_prerules = vendorinfo
    DEPLOYMENT += my_deployment

    ICON = ../qt3d.svg
}
