QT += gui testlib
CONFIG += c++17 console testcase link_pkgconfig
PKGCONFIG += sdl2
TEMPLATE = app
TARGET = tst_sessionruntime
SOURCES += tst_sessionruntime.cpp
INCLUDEPATH += ../../../app ../../../moonlight-common-c/moonlight-common-c/src
