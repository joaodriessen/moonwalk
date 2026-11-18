QT += testlib quick network

CONFIG += c++17 console testcase link_pkgconfig
TEMPLATE = app
TARGET = tst_sessionwindowcontract

PKGCONFIG += sdl2 SDL2_ttf opus

SOURCES += tst_sessionwindowcontract.cpp \
    ../../../app/streaming/connectiondiagnostic.cpp
HEADERS += ../../../app/streaming/connectiondiagnostic.h

INCLUDEPATH += \
    $$PWD/../../../app \
    $$PWD/../../../moonlight-common-c/moonlight-common-c/src \
    $$PWD/../../../qmdnsengine/qmdnsengine/src/include \
    $$PWD/../../../qmdnsengine
