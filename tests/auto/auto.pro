TEMPLATE = subdirs

SUBDIRS += routing

SUBDIRS += \
    smoke \
    platformprofile \
    shellselection \
    plasmaadapter \
    plasmaappearance \
    appearance \
    preferences \
    controller \
    workflows \
    hostlistmodel \
    session \
    boundedreply \
    nvpairing \
    clipboardguard \
    overlaymanager \
    pollingentry

SUBDIRS += supportinfo
SUBDIRS += inputstate clipboardsync
SUBDIRS += sessionruntime
SUBDIRS += launchrequest
SUBDIRS += clipboardimage

SUBDIRS += filetransfer
SUBDIRS += adaptivefeedback

SUBDIRS += systemproperties

SUBDIRS += rendergeometry
SUBDIRS += runtimebitrate \
    automaticbitrate \
    streamplan
