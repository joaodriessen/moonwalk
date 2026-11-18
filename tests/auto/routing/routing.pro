QT += testlib network
CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_routing
INCLUDEPATH += ../../../app
SOURCES += tst_routing.cpp ../../../app/backend/nvaddress.cpp
HEADERS += ../../../app/backend/addressprobe.h
