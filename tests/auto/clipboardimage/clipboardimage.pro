QT += core gui testlib
CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_clipboardimage
INCLUDEPATH += ../../../app
SOURCES += tst_clipboardimage.cpp

CONFIG += link_pkgconfig
PKGCONFIG += sdl2
