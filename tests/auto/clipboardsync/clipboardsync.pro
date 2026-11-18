QT += testlib network
CONFIG += c++17 console testcase link_pkgconfig
TEMPLATE = app
TARGET = tst_clipboardsync
PKGCONFIG += sdl2
INCLUDEPATH += doubles ../../../app
SOURCES += tst_clipboardsync.cpp ../../../app/streaming/clipboardsync.cpp
HEADERS += ../../../app/streaming/clipboardsync.h
