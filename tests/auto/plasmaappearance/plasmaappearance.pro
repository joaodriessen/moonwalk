QT += testlib widgets dbus

CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_plasmaappearanceprovider

DEFINES += STARLIGHT_PLASMA_DESKTOP_ADAPTER

SOURCES += \
    tst_plasmaappearanceprovider.cpp \
    $$PWD/../../../app/platform/plasmaappearanceprovider.cpp

HEADERS += \
    $$PWD/../../../app/platform/plasmaappearanceprovider.h

INCLUDEPATH += $$PWD/../../../app
