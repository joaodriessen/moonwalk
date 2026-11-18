QT += testlib
QT -= gui
CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_automaticbitrate
INCLUDEPATH += ../../../app
SOURCES += tst_automaticbitrate.cpp
HEADERS += ../../../app/streaming/automaticbitrate.h
