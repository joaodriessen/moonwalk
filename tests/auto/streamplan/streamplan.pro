QT += testlib
QT -= gui
CONFIG += c++17 console testcase link_pkgconfig
TEMPLATE = app
TARGET = tst_streamplan
PKGCONFIG += sdl2
INCLUDEPATH += ../../../app ../../../moonlight-common-c/moonlight-common-c/src
SOURCES += tst_streamplan.cpp
HEADERS += \
    ../../../app/streaming/streamadaptation.h \
    ../../../app/streaming/streamplan.h \
    ../../../app/streaming/hostcapacity.h \
    ../../../app/streaming/hostlatencymeter.h \
    ../../../app/streaming/automaticpacing.h \
    ../../../app/streaming/video/ffmpeg-renderers/presentationmonitor.h
