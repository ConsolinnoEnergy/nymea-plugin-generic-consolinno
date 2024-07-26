include(../plugins.pri)

QT += network

SOURCES += \
    integrationplugingenericconsolinno.cpp

HEADERS += \
    integrationplugingenericconsolinno.h

LIBS += -ljsonrpccpp-common -ljsonrpccpp-client -ljsoncpp
