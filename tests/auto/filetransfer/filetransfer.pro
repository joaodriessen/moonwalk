QT += core gui testlib network qml
CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_filetransfer
INCLUDEPATH += doubles ../../../app
SOURCES += tst_filetransfer.cpp ../../../app/streaming/filetransfer.cpp
HEADERS += ../../../app/streaming/filetransfer.h

DEFINES += FILE_TRANSFER_TESTING
