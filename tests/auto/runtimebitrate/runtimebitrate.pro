QT += testlib network
CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_runtimebitrate
INCLUDEPATH += doubles ../../../app
SOURCES += tst_runtimebitrate.cpp ../../../app/streaming/runtimebitrate.cpp
HEADERS += ../../../app/streaming/runtimebitrate.h
