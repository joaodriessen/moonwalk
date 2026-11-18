QT += core gui network testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = tst_supportinfo
SOURCES += tst_supportinfo.cpp ../../../app/supportinfo.cpp
HEADERS += ../../../app/supportinfo.h
