QT += testlib gui

linux {
    QT += dbus
    DEFINES += STARLIGHT_GNOME_DESKTOP_ADAPTER
}

CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_appearanceprovider

SOURCES += \
    tst_appearanceprovider.cpp \
    $$PWD/../../../app/platform/appearanceprovider.cpp \
    $$PWD/../../../app/gui/adaptive/gnome/gnomechromeselector.cpp

HEADERS += \
    $$PWD/../../../app/platform/appearanceprovider.h \
    $$PWD/../../../app/gui/adaptive/gnome/gnomechromeselector.h

INCLUDEPATH += $$PWD/../../../app
