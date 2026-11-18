QT += testlib network gui
CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_nvpairing
SOURCES += tst_nvpairing.cpp \
    ../../../app/backend/nvpairingmanager.cpp \
    ../../../app/backend/nvhttp.cpp \
    ../../../app/backend/nvaddress.cpp \
    ../../../app/backend/identitymanager.cpp
HEADERS += ../../../app/backend/nvhttp.h
INCLUDEPATH += ../../../app ../../../moonlight-common-c/moonlight-common-c/src
LIBS += -lssl -lcrypto
