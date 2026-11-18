# Developer-only visual tool. Not part of tests.pro, the app or any package.
QT += core gui quick qml quickcontrols2 widgets svg
CONFIG += c++17
TEMPLATE = app
TARGET = starlight-preview

SOURCES += preview.cpp \
    ../../app/application/hostworkflow.cpp \
    ../../app/application/pollinglifecycle.cpp \
    ../../app/gui/hostlistmodel.cpp \
    ../../app/gui/lucideiconprovider.cpp
HEADERS += ../qml/testhostsource.h \
    ../../app/application/hostworkflow.h \
    ../../app/application/pollinglifecycle.h \
    ../../app/gui/hostidentitysource.h \
    ../../app/gui/hostlistmodel.h \
    ../../app/gui/lucideiconprovider.h
INCLUDEPATH += ../../app ../qml
RESOURCES += ../../app/resources.qrc
DEFINES += PREVIEW_SOURCE_DIR=\\\"$$PWD\\\"
