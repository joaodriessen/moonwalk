QT += testlib network qml

CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_pollingentry

SOURCES += tst_pollingentry.cpp

INCLUDEPATH += \
    $$PWD/../../../app \
    $$PWD/../../../moonlight-common-c/moonlight-common-c/src \
    $$PWD/../../../qmdnsengine/qmdnsengine/src/include \
    $$PWD/../../../qmdnsengine
