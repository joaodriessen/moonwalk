QT += testlib quick qml qmltest svg

CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = tst_qmlcharacterization

SOURCES += \
    tst_qml.cpp \
    $$PWD/../../app/application/hostworkflow.cpp \
    $$PWD/../../app/application/pollinglifecycle.cpp \
    $$PWD/../../app/gui/hostlistmodel.cpp \
    $$PWD/../../app/gui/lucideiconprovider.cpp

HEADERS += \
    $$PWD/../../app/application/hostworkflow.h \
    $$PWD/../../app/gui/hostidentitysource.h \
    $$PWD/../../app/gui/hostlistmodel.h \
    $$PWD/../../app/gui/lucideiconprovider.h \
    $$PWD/testhostsource.h \
    $$PWD/../../app/application/materialstylebootstrap.h \
    $$PWD/../../app/application/pollinglifecycle.h

INCLUDEPATH += $$PWD/../../app

RESOURCES += \
    $$PWD/../../app/resources.qrc \
    $$PWD/pagecontract.qrc

DEFINES += \
    QUICK_TEST_SOURCE_DIR=\\\"$$PWD\\\" \
    TASK3_QML_IMPORT_PATH=\\\"$$PWD/imports\\\"

DISTFILES += \
    TestCompat.js \
    pagecontract.qrc \
    tst_facade_boundary.qml \
    tst_nativeparity.qml \
    tst_gnomesettings.qml \
    ../../app/gui/adaptive/gnome/GnomeAboutDialog.qml \
    tst_plasmasettings.qml \
    tst_gnomepages.qml \
    tst_plasmapages.qml \
    tst_hostlistmodel.qml \
    tst_pageroutes.qml \
    tst_pagecontexts.qml \
    tst_windoweffects.qml \
    tst_sessioncontrols.qml \
    fixtures/FailingStreamSegue.qml \
    fixtures/GnomeHostFacade.qml \
    fixtures/SettingsFacade.qml \
    fixtures/WindowEffectRoute.qml \
    tst_settingspagecontext.qml \
    ../data/page-contract-v1.json \
    fixtures/plasma-breeze/kdeglobals \
    fixtures/plasma-fusion/kdeglobals \
    fixtures/plasma-incomplete-style/org/kde/desktop/qmldir \
    fixtures/plasma-reduced-motion/kdeglobals
