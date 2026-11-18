QT += core gui svg testlib
CONFIG += c++17 console testcase link_pkgconfig
PKGCONFIG += sdl2 SDL2_ttf
TEMPLATE = app
TARGET = tst_overlaymanager
INCLUDEPATH += ../../../app
SOURCES += tst_overlaymanager.cpp ../../../app/streaming/video/overlaymanager.cpp
HEADERS += ../../../app/streaming/video/overlaymanager.h ../../../app/path.h ../../../app/streaming/streamtoolbar.h
