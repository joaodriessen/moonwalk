QT += quickcontrols2 quick qml widgets

CONFIG += c++17 console
TEMPLATE = app
TARGET = tst_plasma_style_import_probe_helper

SOURCES += \
    main.cpp \
    $$PWD/../../../../app/gui/adaptive/plasma/plasmastyleprobe.cpp

HEADERS += \
    $$PWD/../../../../app/gui/adaptive/plasma/plasmastyleprobe.h

INCLUDEPATH += $$PWD/../../../../app
