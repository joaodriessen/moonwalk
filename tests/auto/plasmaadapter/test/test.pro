QT += testlib

CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_plasmaadapter
DESTDIR = ..

SOURCES += \
    ../tst_plasmaadapter.cpp \
    $$PWD/../../../../app/gui/adaptive/plasma/plasmadesktopgate.cpp \
    $$PWD/../../../../app/gui/adaptive/plasma/plasmastyleprobe.cpp

HEADERS += \
    $$PWD/../../../../app/gui/adaptive/plasma/plasmadesktopgate.h \
    $$PWD/../../../../app/gui/adaptive/plasma/plasmastyleprobe.h

INCLUDEPATH += $$PWD/../../../../app
