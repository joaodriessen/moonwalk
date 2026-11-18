QT += testlib

CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_adaptiveshellselection

SOURCES += tst_adaptiveshellselection.cpp

HEADERS += \
    $$PWD/../../../app/application/adaptiveshellselection.h \
    $$PWD/../../../app/application/materialstylebootstrap.h

INCLUDEPATH += $$PWD/../../../app
