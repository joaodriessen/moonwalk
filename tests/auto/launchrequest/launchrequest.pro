QT += core gui network testlib
CONFIG += c++17 console testcase link_pkgconfig
TEMPLATE = app
TARGET = tst_launchrequest
PKGCONFIG += openssl sdl2
INCLUDEPATH += ../../../app ../../../moonlight-common-c/moonlight-common-c/src ../../../qmdnsengine/qmdnsengine/src/include ../../../qmdnsengine
SOURCES += tst_launchrequest.cpp ../../../app/backend/nvhttp.cpp ../../../app/backend/nvaddress.cpp ../../../app/backend/identitymanager.cpp
HEADERS += ../../../app/backend/nvhttp.h
