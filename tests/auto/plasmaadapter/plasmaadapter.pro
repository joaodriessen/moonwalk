TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
    fixtureplugin \
    styleimportprobe \
    test

styleimportprobe.depends = fixtureplugin
test.depends = fixtureplugin styleimportprobe
