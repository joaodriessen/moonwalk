QT += testlib qml

CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_workflows

SOURCES += \
    tst_workflows.cpp \
    $$PWD/../../../app/backend/nvapp.cpp \
    $$PWD/../../../app/application/controllerintent.cpp \
    $$PWD/../../../app/application/hostworkflow.cpp \
    $$PWD/../../../app/application/languageworkflow.cpp \
    $$PWD/../../../app/application/pollinglifecycle.cpp \
    $$PWD/../../../app/application/preferencesworkflow.cpp

HEADERS += \
    $$PWD/../../../app/application/controllerintent.h \
    $$PWD/../../../app/application/hostworkflow.h \
    $$PWD/../../../app/application/languageworkflow.h \
    $$PWD/../../../app/application/pollinglifecycle.h \
    $$PWD/../../../app/application/preferencesworkflow.h

INCLUDEPATH += $$PWD/../../../app

RESOURCES += $$PWD/../../../app/resources.qrc

unix:!macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += sdl2
}
