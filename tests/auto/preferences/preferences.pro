QT += testlib qml

CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_streamingpreferences

SOURCES += \
    tst_streamingpreferences.cpp \
    $$PWD/../../../app/application/languageworkflow.cpp \
    $$PWD/../../../app/application/preferencesworkflow.cpp \
    $$PWD/../../../app/settings/streamingpreferences.cpp \
    $$PWD/../../../app/cli/commandlineparser.cpp \
    $$PWD/../../../app/supportinfo.cpp \
    $$PWD/../../../app/wm.cpp

HEADERS += \
    $$PWD/../../../app/settings/streamingpreferences.h \
    $$PWD/../../../app/application/languageworkflow.h \
    $$PWD/../../../app/application/preferencesworkflow.h \
    $$PWD/../../../app/cli/commandlineparser.h \
    $$PWD/../../../app/supportinfo.h \
    $$PWD/../../../app/utils.h

INCLUDEPATH += $$PWD/../../../app $$PWD/../../../moonlight-common-c/moonlight-common-c/src

unix:!macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += sdl2
}

DEFINES += STARLIGHT_VERSION_STR=\\\"test\\\" VERSION_STR=\\\"test\\\"
