QT += testlib

CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_platformprofile

SOURCES += \
    tst_platformprofile.cpp \
    $$PWD/../../../app/platform/platformprofileresolver.cpp

HEADERS += \
    $$PWD/../../../app/platform/platformprofile.h \
    $$PWD/../../../app/platform/platformprofileresolver.h

INCLUDEPATH += $$PWD/../../../app
