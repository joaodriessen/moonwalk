QT += testlib qml

CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_controllerkeymapper

SOURCES += \
    tst_controllerkeymapper.cpp \
    $$PWD/../../../app/application/controllerintent.cpp

HEADERS += $$PWD/../../../app/application/controllerintent.h

INCLUDEPATH += $$PWD/../../../app

unix:!macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += sdl2
}
