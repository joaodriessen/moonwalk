QT += core testlib
CONFIG += c++17 console testcase link_pkgconfig release
CONFIG -= debug
TEMPLATE = app
TARGET = tst_adaptivefeedback
PKGCONFIG += openssl
COMMON = ../../../moonlight-common-c/moonlight-common-c
INCLUDEPATH += $$COMMON/src $$COMMON/enet/include $$COMMON/nanors $$COMMON/nanors/deps $$COMMON/nanors/deps/obl
SOURCES += tst_adaptivefeedback.cpp queue.c transport.c \
 $$COMMON/src/Platform.c $$COMMON/src/PlatformCrypto.c $$COMMON/src/Misc.c $$COMMON/src/LinkedBlockingQueue.c \
 $$COMMON/enet/callbacks.c $$COMMON/enet/packet.c $$COMMON/enet/list.c \
 $$COMMON/nanors/rs.c $$COMMON/nanors/deps/obl/oblas_common.c $$COMMON/nanors/deps/obl/oblas_lite.c
DEFINES += HAS_SOCKLEN_T HAVE_CLOCK_GETTIME=1
unix:!macx {
 QMAKE_CFLAGS += -std=gnu99 -ffunction-sections -fdata-sections
 QMAKE_LFLAGS += -Wl,--gc-sections
 LIBS += -pthread
}
macx {
 QMAKE_CFLAGS += -ffunction-sections -fdata-sections
 QMAKE_LFLAGS += -Wl,-dead_strip
}
# Optional private integration gate: no host repository dependency in ordinary
# client builds/tests. Uses the real host parser/controller when supplied.
!isEmpty(ADAPTIVE_HOST_SOURCE) {
    INCLUDEPATH += $$ADAPTIVE_HOST_SOURCE/src
    DEFINES += ADAPTIVE_HOST_CONTROLLER
}
