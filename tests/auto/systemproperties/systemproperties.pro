QT += core gui testlib
CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_systemproperties
INCLUDEPATH += doubles ../../../app
SOURCES += tst_systemproperties.cpp ../../../app/backend/systemproperties.cpp
HEADERS += ../../../app/backend/systemproperties.h
DEFINES += DECODER_PROBE_TIMEOUT_MS=30
