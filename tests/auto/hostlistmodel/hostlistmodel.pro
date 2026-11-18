QT += testlib qml

CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_hostlistmodel

SOURCES += \
    tst_hostlistmodel.cpp \
    $$PWD/../../../app/gui/hostlistmodel.cpp

HEADERS += \
    $$PWD/../../../app/gui/hostidentitysource.h \
    $$PWD/../../../app/gui/hostlistmodel.h

INCLUDEPATH += $$PWD/../../../app
