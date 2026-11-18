QT += core quick network quickcontrols2 svg
CONFIG += c++17

unix:!macx {
    TARGET = starlight
} else {
    # On macOS, this is the name displayed in the global menu bar
    TARGET = Starlight
}

include(../globaldefs.pri)

# QApplication and the KDE desktop style are compiled only into ordinary
# desktop-Linux builds. Steam Link/config_SL, generic embedded builds, Android,
# macOS, and Windows never acquire a QtWidgets dependency.
linux:!android:!config_SL:!embedded {
    QT += widgets dbus
    DEFINES += STARLIGHT_PLASMA_DESKTOP_ADAPTER STARLIGHT_GNOME_DESKTOP_ADAPTER
}

# Precompile QML files to avoid writing qmlcache on portable versions.
# Since this binds the app against the Qt runtime version, we will only
# do this for Windows and Mac (when disable-prebuilts is not defined),
# since they always ship with the matching build of the Qt runtime.
!disable-prebuilts {
    win32|macx {
        CONFIG(release, debug|release) {
            CONFIG += qtquickcompiler
        }
    }
}

TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

win32 {
    !exists($$PWD/../libs/windows) {
        error("Missing dependencies. Please run 'powershell .\setup-deps.ps1' to fetch prebuilt libraries.")
    }

    contains(QT_ARCH, x86_64) {
        LIBS += -L$$PWD/../libs/windows/lib/x64
        INCLUDEPATH += $$PWD/../libs/windows/include/x64 $$PWD/../libs/windows/include/x64/SDL2
    }
    contains(QT_ARCH, arm64) {
        LIBS += -L$$PWD/../libs/windows/lib/arm64
        INCLUDEPATH += $$PWD/../libs/windows/include/arm64 $$PWD/../libs/windows/include/arm64/SDL2
    }

    INCLUDEPATH += $$PWD/../libs/windows/include
    LIBS += ws2_32.lib winmm.lib dxva2.lib ole32.lib gdi32.lib user32.lib d3d9.lib dwmapi.lib dbghelp.lib
}
macx:!disable-prebuilts {
    !exists($$PWD/../libs/mac) {
        error("Missing dependencies. Please run 'python3 setup-deps.py' to fetch prebuilt libraries.")
    }

    INCLUDEPATH += $$PWD/../libs/mac/include $$PWD/../libs/mac/include/SDL2
    LIBS += -L$$PWD/../libs/mac/lib
}

unix:if(!macx|disable-prebuilts) {
    CONFIG += link_pkgconfig
    PKGCONFIG += openssl sdl2 SDL2_ttf

    # We have our own optimized libopus.a for Steam Link
    if(!config_SL|disable-prebuilts) {
        PKGCONFIG += opus
    }

    !disable-ffmpeg {
        packagesExist(libavcodec) {
            PKGCONFIG += libavcodec libavutil libswscale
            CONFIG += ffmpeg

            !disable-libva {
                packagesExist(libva) {
                    !disable-x11 {
                        packagesExist(libva-x11) {
                            CONFIG += libva-x11
                        }
                    }
                    !disable-wayland {
                        packagesExist(libva-wayland) {
                            CONFIG += libva-wayland
                        }
                    }
                    # libva-drm provides VA-API's DRM render-node display path
                    # (vaGetDisplayDRM), which is REQUIRED for VAAPI on modern Wayland where
                    # vaGetDisplayWl() no longer works (Mesa dropped wl_drm_interface). It is
                    # independent of the DRM-master output renderer (disable-libdrm), so it has
                    # its own opt-out and is NOT gated on disable-libdrm.
                    !disable-libva-drm {
                        packagesExist(libva-drm) {
                            CONFIG += libva-drm
                        }
                    }
                    CONFIG += libva
                }
            }

            !disable-libvdpau {
                packagesExist(vdpau) {
                    CONFIG += libvdpau
                }
            }

            !disable-mmal {
                packagesExist(mmal) {
                    PKGCONFIG += mmal
                    CONFIG += mmal
                }
            }

            !disable-libdrm {
                packagesExist(libdrm) {
                    PKGCONFIG += libdrm
                    CONFIG += libdrm
                }
            }

            # Disabled by default due to reliability issues. See #1314.
            # CUDA interop is superseded by VDPAU and Vulkan Video.
            enable-cuda {
                packagesExist(ffnvcodec) {
                    PKGCONFIG += ffnvcodec
                    CONFIG += cuda
                }
            }

            !disable-libplacebo {
                packagesExist(libplacebo) {
                    PKGCONFIG += libplacebo
                    CONFIG += libplacebo
                }
            }
        }

        !disable-wayland {
            packagesExist(wayland-client) {
                CONFIG += wayland
                PKGCONFIG += wayland-client
            }
        }

        !disable-x11 {
            packagesExist(x11) {
                DEFINES += HAS_X11
                PKGCONFIG += x11
            }
        }
    }
}
win32 {
    LIBS += -llibssl -llibcrypto -lSDL2 -lSDL2_ttf -lavcodec -lavutil -lswscale -lopus -ldxgi -ld3d11 -llibplacebo
    CONFIG += ffmpeg libplacebo
}
win32:!winrt {
    CONFIG += discord-rpc
}
macx {
    !disable-prebuilts {
        LIBS += -lssl.3 -lcrypto.3 -lavcodec.62 -lavutil.60 -lswscale.9 -lopus.0 -lSDL2 -lSDL2_ttf -lplacebo
        CONFIG += discord-rpc libplacebo
    }

    LIBS += -lobjc -framework VideoToolbox -framework AVFoundation -framework CoreVideo -framework CoreGraphics -framework CoreMedia -framework AppKit -framework Metal -framework QuartzCore
    CONFIG += ffmpeg
}

SOURCES += \
    backend/nvaddress.cpp \
    backend/nvapp.cpp \
    application/controllerintent.cpp \
    application/hostworkflow.cpp \
    application/languageworkflow.cpp \
    application/pollinglifecycle.cpp \
    application/preferencesworkflow.cpp \
    gui/adaptive/gnome/gnomechromeselector.cpp \
    gui/adaptive/plasma/plasmadesktopgate.cpp \
    gui/adaptive/plasma/plasmastyleprobe.cpp \
    cli/pair.cpp \
    platform/appearanceprovider.cpp \
    platform/plasmaappearanceprovider.cpp \
    platform/platformprofileresolver.cpp \
    main.cpp \
    backend/computerseeker.cpp \
    backend/identitymanager.cpp \
    backend/nvcomputer.cpp \
    backend/nvhttp.cpp \
    backend/nvpairingmanager.cpp \
    backend/computermanager.cpp \
    backend/boxartmanager.cpp \
    backend/richpresencemanager.cpp \
    cli/commandlineparser.cpp \
    cli/listapps.cpp \
    cli/quitstream.cpp \
    cli/startstream.cpp \
    settings/compatfetcher.cpp \
    settings/mappingfetcher.cpp \
    settings/streamingpreferences.cpp \
    streaming/input/abstouch.cpp \
    streaming/input/gamepad.cpp \
    streaming/input/input.cpp \
    streaming/input/keyboard.cpp \
    streaming/input/mouse.cpp \
    streaming/input/reltouch.cpp \
    streaming/session.cpp \
    streaming/filetransfer.cpp \
    streaming/connectiondiagnostic.cpp \
    streaming/clipboardsync.cpp \
    streaming/runtimebitrate.cpp \
    streaming/automaticbitratedriver.cpp \
    streaming/video/ffmpeg-renderers/presentationmonitor.cpp \
    streaming/cursorsync.cpp \
    streaming/audio/audio.cpp \
    streaming/audio/renderers/sdlaud.cpp \
    gui/computermodel.cpp \
    gui/hostlistmodel.cpp \
    gui/lucideiconprovider.cpp \
    streaming/bandwidth.cpp \
    streaming/streamutils.cpp \
    streaming/windowdecoration.cpp \
    backend/autoupdatechecker.cpp \
    path.cpp \
    settings/mappingmanager.cpp \
    gui/sdlgamepadkeynavigation.cpp \
    streaming/video/overlaymanager.cpp \
    backend/systemproperties.cpp \
    wm.cpp

HEADERS += \
    backend/displaysnapshot.h \
    SDL_compat.h \
    backend/nvaddress.h \
    backend/nvapp.h \
    application/adaptiveshellselection.h \
    application/materialstylebootstrap.h \
    application/controllerintent.h \
    application/hostworkflow.h \
    application/languageworkflow.h \
    application/pollinglifecycle.h \
    application/preferencesworkflow.h \
    gui/adaptive/gnome/gnomechromeselector.h \
    gui/adaptive/plasma/plasmadesktopgate.h \
    gui/adaptive/plasma/plasmastyleprobe.h \
    cli/pair.h \
    platform/appearanceprovider.h \
    platform/plasmaappearanceprovider.h \
    platform/platformprofile.h \
    platform/platformprofileresolver.h \
    settings/compatfetcher.h \
    settings/mappingfetcher.h \
    utils.h \
    backend/computerseeker.h \
    backend/identitymanager.h \
    backend/nvcomputer.h \
    backend/nvhttp.h \
    backend/nvpairingmanager.h \
    backend/computermanager.h \
    backend/boxartmanager.h \
    backend/richpresencemanager.h \
    cli/commandlineparser.h \
    cli/listapps.h \
    cli/quitstream.h \
    cli/startstream.h \
    settings/streamingpreferences.h \
    streaming/input/input.h \
    streaming/session.h \
    streaming/filetransfer.h \
    streaming/pngclipboard.h \
    streaming/clientclipboardimage.h \
    streaming/nativeclipboard.h \
    streaming/sessionretrypolicy.h \
    streaming/desktopscalepolicy.h \
    streaming/input/localtextinputpolicy.h \
    streaming/connectiondiagnostic.h \
    streaming/cursormodepolicy.h \
    streaming/cursorsizepolicy.h \
    streaming/windowgeometrypolicy.h \
    streaming/windowdecoration.h \
    streaming/streamstatus.h \
    streaming/input/clipboardtext.h \
    streaming/input/forwardedmousebuttons.h \
    streaming/input/wheelaccumulator.h \
    backend/audiolayoutpolicy.h \
    streaming/clipboardsync.h \
    streaming/runtimebitrate.h \
    streaming/automaticbitrate.h \
    streaming/hostcapacity.h \
    streaming/hostlatencymeter.h \
    streaming/streamplan.h \
    streaming/streamadaptation.h \
    streaming/streamrecommendation.h \
    streaming/automaticpacing.h \
    streaming/automaticbitratedriver.h \
    streaming/video/ffmpeg-renderers/presentationmonitor.h \
    streaming/cursorsync.h \
    streaming/audio/renderers/renderer.h \
    streaming/audio/renderers/sdl.h \
    gui/computermodel.h \
    gui/hostidentitysource.h \
    gui/hostlistmodel.h \
    gui/lucideiconprovider.h \
    streaming/video/decoder.h \
    streaming/bandwidth.h \
    streaming/streamutils.h \
    backend/autoupdatechecker.h \
    path.h \
    settings/mappingmanager.h \
    gui/sdlgamepadkeynavigation.h \
    streaming/video/overlaymanager.h \
    backend/systemproperties.h

# Platform-specific renderers and decoders
ffmpeg {
    message(FFmpeg decoder selected)

    DEFINES += HAVE_FFMPEG
    SOURCES += \
        streaming/video/ffmpeg.cpp \
        streaming/video/ffmpeg-renderers/genhwaccel.cpp \
        streaming/video/ffmpeg-renderers/sdlvid.cpp \
        streaming/video/ffmpeg-renderers/swframemapper.cpp \
        streaming/video/ffmpeg-renderers/pacer/pacer.cpp

    HEADERS += \
        streaming/video/ffmpeg.h \
        streaming/video/ffmpeg-renderers/renderer.h \
        streaming/video/ffmpeg-renderers/genhwaccel.h \
        streaming/video/ffmpeg-renderers/sdlvid.h \
        streaming/video/ffmpeg-renderers/swframemapper.h \
        streaming/video/ffmpeg-renderers/pacer/pacer.h
}
libva {
    message(VAAPI renderer selected)

    PKGCONFIG += libva
    DEFINES += HAVE_LIBVA
    SOURCES += streaming/video/ffmpeg-renderers/vaapi.cpp
    HEADERS += streaming/video/ffmpeg-renderers/vaapi.h
}
libva-x11 {
    message(VAAPI X11 support enabled)

    PKGCONFIG += libva-x11
    DEFINES += HAVE_LIBVA_X11
}
libva-wayland {
    message(VAAPI Wayland support enabled)

    PKGCONFIG += libva-wayland
    DEFINES += HAVE_LIBVA_WAYLAND
}
libva-drm {
    message(VAAPI DRM support enabled)

    PKGCONFIG += libva-drm
    DEFINES += HAVE_LIBVA_DRM
}
libvdpau {
    message(VDPAU renderer selected)

    DEFINES += HAVE_LIBVDPAU
    SOURCES += streaming/video/ffmpeg-renderers/vdpau.cpp
    HEADERS += streaming/video/ffmpeg-renderers/vdpau.h
}
mmal {
    message(MMAL renderer selected)

    DEFINES += HAVE_MMAL
    SOURCES += streaming/video/ffmpeg-renderers/mmal.cpp
    HEADERS += streaming/video/ffmpeg-renderers/mmal.h

    # We suppress EGL usage when MMAL is available because MMAL has
    # significantly better performance than EGL on the Pi. Setting
    # this option allows EGL usage even if built with MMAL support.
    #
    # It is highly recommended to also build with 'gpuslow' to avoid
    # EGL being preferred if direct DRM rendering is available.
    allow-egl-with-mmal {
        message(Allowing EGL usage with MMAL enabled)

        DEFINES += ALLOW_EGL_WITH_MMAL
    }
}
libdrm {
    message(DRM renderer selected)

    DEFINES += HAVE_DRM
    SOURCES += streaming/video/ffmpeg-renderers/drm.cpp
    HEADERS += streaming/video/ffmpeg-renderers/drm.h

    linux {
        !disable-masterhooks {
            message(Master hooks enabled)
            DEFINES += HAVE_DRM_MASTER_HOOKS
            SOURCES += masterhook.c masterhook_internal.c
            LIBS += -ldl -pthread
        }
    }
}
cuda {
    message(CUDA support enabled)

    DEFINES += HAVE_CUDA
    SOURCES += streaming/video/ffmpeg-renderers/cuda.cpp
    HEADERS += streaming/video/ffmpeg-renderers/cuda.h

    # ffnvcodec uses libdl in cuda_load_functions()/cuda_free_functions()
    LIBS += -ldl
}
libplacebo {
    message(Vulkan support enabled via libplacebo)

    DEFINES += HAVE_LIBPLACEBO_VULKAN
    SOURCES += \
        streaming/video/ffmpeg-renderers/plvk.cpp \
        streaming/video/ffmpeg-renderers/plvk_c.c
    HEADERS += \
        streaming/video/ffmpeg-renderers/plvk.h

    macx {
        SOURCES += streaming/video/ffmpeg-renderers/plvk_objc.mm
    }
}
config_EGL {
    message(EGL renderer selected)

    CONFIG += egl
    DEFINES += HAVE_EGL
    SOURCES += \
        streaming/video/ffmpeg-renderers/eglvid.cpp \
        streaming/video/ffmpeg-renderers/egl_extensions.cpp \
        streaming/video/ffmpeg-renderers/eglimagefactory.cpp
    HEADERS += \
        streaming/video/ffmpeg-renderers/eglvid.h \
        streaming/video/ffmpeg-renderers/eglimagefactory.h
}
config_SL {
    message(Steam Link build configuration selected)

    !disable-prebuilts {
        # Link against our NEON-optimized libopus build
        LIBS += -L$$PWD/../libs/steamlink/lib
        INCLUDEPATH += $$PWD/../libs/steamlink/include
        LIBS += -lopus -larmasm -lNE10
    }

    DEFINES += EMBEDDED_BUILD STEAM_LINK HAVE_SLVIDEO HAVE_SLAUDIO
    LIBS += -lSLVideo -lSLAudio

    SOURCES += \
        streaming/video/slvid.cpp \
        streaming/audio/renderers/slaud.cpp
    HEADERS += \
        streaming/video/slvid.h \
        streaming/audio/renderers/slaud.h
}
win32 {
    HEADERS += streaming/video/ffmpeg-renderers/dxutil.h
}
win32:!winrt {
    message(DXVA2 and D3D11VA renderers selected)

    SOURCES += \
        streaming/video/ffmpeg-renderers/dxva2.cpp \
        streaming/video/ffmpeg-renderers/d3d11va.cpp \
        streaming/video/ffmpeg-renderers/pacer/dxvsyncsource.cpp

    HEADERS += \
        streaming/video/ffmpeg-renderers/dxva2.h \
        streaming/video/ffmpeg-renderers/d3d11va.h \
        streaming/video/ffmpeg-renderers/pacer/dxvsyncsource.h
}
macx {
    message(VideoToolbox renderer selected)

    SOURCES += \
        streaming/video/ffmpeg-renderers/vt_base.mm \
        streaming/video/ffmpeg-renderers/vt_avsamplelayer.mm \
        streaming/video/ffmpeg-renderers/vt_metal.mm

    HEADERS += \
        streaming/video/ffmpeg-renderers/vt.h
}
discord-rpc {
    message(Discord integration enabled)

    LIBS += -ldiscord-rpc
    DEFINES += HAVE_DISCORD
}
embedded {
    message(Embedded build)

    DEFINES += EMBEDDED_BUILD
}
glslow {
    message(GL slow build)

    DEFINES += GL_IS_SLOW
}
vkslow {
    message(Vulkan slow build)

    DEFINES += VULKAN_IS_SLOW
}
gpuslow {
    message(GPU slow build)

    DEFINES += GL_IS_SLOW VULKAN_IS_SLOW
}
wayland {
    message(Wayland extensions enabled)

    DEFINES += HAS_WAYLAND
    SOURCES += streaming/video/ffmpeg-renderers/pacer/waylandvsyncsource.cpp
    HEADERS += streaming/video/ffmpeg-renderers/pacer/waylandvsyncsource.h
}

RESOURCES += \
    resources.qrc \
    qml.qrc

DISTFILES += \
    gui/adaptive/AdaptiveHostSlice.qml \
    gui/adaptive/HostPageRoute.qml \
    gui/adaptive/HostPageContext.qml \
    gui/SessionControls.qml \
    gui/FileTransferPane.qml \
    gui/FileTransferPicker.qml \
    gui/adaptive/HostPageFacade.qml \
    gui/adaptive/SettingsPageContext.qml \
    gui/adaptive/SettingsPageRoute.qml \
    gui/adaptive/SettingsPageFacade.qml \
    gui/adaptive/PlatformAdapter.qml \
    gui/adaptive/generic/GenericPlatformAdapter.qml \
    gui/adaptive/generic/GenericSettingsSlice.qml \
    gui/style/StarlightAdwaita/BusyIndicator.qml \
    gui/style/StarlightAdwaita/Button.qml \
    gui/style/StarlightAdwaita/CheckBox.qml \
    gui/style/StarlightAdwaita/ComboBox.qml \
    gui/style/StarlightAdwaita/Dialog.qml \
    gui/style/StarlightAdwaita/DialogButtonBox.qml \
    gui/style/StarlightAdwaita/Frame.qml \
    gui/style/StarlightAdwaita/ItemDelegate.qml \
    gui/style/StarlightAdwaita/Label.qml \
    gui/style/StarlightAdwaita/Menu.qml \
    gui/style/StarlightAdwaita/MenuItem.qml \
    gui/style/StarlightAdwaita/MenuSeparator.qml \
    gui/style/StarlightAdwaita/ProgressBar.qml \
    gui/style/StarlightAdwaita/RadioButton.qml \
    gui/style/StarlightAdwaita/ScrollBar.qml \
    gui/style/StarlightAdwaita/Slider.qml \
    gui/style/StarlightAdwaita/SpinBox.qml \
    gui/style/StarlightAdwaita/Switch.qml \
    gui/style/StarlightAdwaita/TabBar.qml \
    gui/style/StarlightAdwaita/TabButton.qml \
    gui/style/StarlightAdwaita/TextArea.qml \
    gui/style/StarlightAdwaita/TextField.qml \
    gui/style/StarlightAdwaita/ToolButton.qml \
    gui/style/StarlightAdwaita/ToolTip.qml \
    gui/SessionRow.qml \
    gui/SessionStatusView.qml \
    gui/adaptive/settings/AppSettingsModel.qml \
    gui/adaptive/settings/HostDialogState.qml \
    gui/adaptive/settings/HostPresentation.qml \
    gui/adaptive/settings/SettingGroup.qml \
    gui/adaptive/settings/SettingRow.qml \
    gui/adaptive/gnome/GnomeRowFill.qml \
    gui/adaptive/gnome/GnomeSettingRow.qml \
    gui/adaptive/gnome/GnomeExpanderRow.qml \
    gui/adaptive/gnome/GnomeSettingsList.qml \
    gui/adaptive/gnome/GnomeStatusPage.qml \
    gui/adaptive/gnome/GnomePreferencesDialog.qml \
    gui/adaptive/plasma/PlasmaSettingsForm.qml \
    gui/adaptive/gnome/GnomePlatformAdapter.qml \
    gui/adaptive/gnome/GnomeHostSlice.qml \
    gui/adaptive/gnome/GnomeHostsPage.qml \
    gui/adaptive/gnome/GnomeHostDialogLayer.qml \
    gui/adaptive/gnome/GnomeSettingsPage.qml \
    gui/adaptive/gnome/GnomeSettingsSlice.qml \
    gui/adaptive/gnome/GnomeTokens.qml \
    gui/adaptive/gnome/GnomeButton.qml \
    gui/adaptive/gnome/GnomeDropdown.qml \
    gui/adaptive/gnome/GnomeToggle.qml \
    gui/adaptive/gnome/GnomeDialog.qml \
    gui/adaptive/gnome/GnomeAboutDialog.qml \
    gui/adaptive/gnome/GnomeMenu.qml \
    gui/adaptive/gnome/GnomeTextField.qml \
    gui/adaptive/gnome/GnomeSlider.qml \
    gui/adaptive/gnome/GnomeScrollBar.qml \
    gui/adaptive/gnome/GnomeBusyIndicator.qml \
    gui/adaptive/gnome/GnomeNavigationButton.qml \
    gui/adaptive/gnome/GnomeGroup.qml \
    gui/adaptive/gnome/GnomeRowBase.qml \
    gui/adaptive/gnome/GnomeSwitchRow.qml \
    gui/adaptive/gnome/GnomeComboRow.qml \
    gui/adaptive/gnome/GnomeSliderRow.qml \
    gui/adaptive/gnome/GnomeActionRow.qml \
    gui/adaptive/gnome/GnomeSidebarRow.qml \
    gui/adaptive/plasma/PlasmaPlatformAdapter.qml \
    gui/adaptive/plasma/PlasmaPalette.qml \
    gui/adaptive/plasma/PlasmaHostSlice.qml \
    gui/adaptive/plasma/PlasmaHostsPage.qml \
    gui/adaptive/plasma/PlasmaHostDialogLayer.qml \
    gui/adaptive/plasma/PlasmaSettingsPage.qml \
    gui/adaptive/plasma/PlasmaCard.qml \
    gui/adaptive/plasma/PlasmaFormRow.qml \
    gui/adaptive/plasma/PlasmaSwitchRow.qml \
    gui/adaptive/plasma/PlasmaComboRow.qml \
    gui/adaptive/plasma/PlasmaSliderRow.qml \
    gui/adaptive/plasma/PlasmaButtonRow.qml \
    gui/adaptive/plasma/PlasmaListItem.qml \
    gui/adaptive/plasma/PlasmaSettingsSlice.qml \
    gui/adaptive/plasma/PlasmaButton.qml \
    gui/adaptive/plasma/PlasmaDropdown.qml \
    gui/adaptive/plasma/PlasmaToggle.qml \
    gui/adaptive/plasma/PlasmaAboutDialog.qml \
    gui/adaptive/plasma/PlasmaDialog.qml \
    gui/adaptive/plasma/PlasmaMenu.qml \
    gui/adaptive/plasma/PlasmaTextField.qml \
    gui/adaptive/plasma/PlasmaSlider.qml \
    gui/adaptive/plasma/PlasmaScrollBar.qml \
    gui/adaptive/plasma/PlasmaBusyIndicator.qml \
    gui/adaptive/plasma/PlasmaNavigationButton.qml

TRANSLATIONS += \
    languages/qml_zh_CN.ts \
    languages/qml_de.ts \
    languages/qml_fr.ts \
    languages/qml_nb_NO.ts \
    languages/qml_ru.ts \
    languages/qml_es.ts \
    languages/qml_ja.ts \
    languages/qml_vi.ts \
    languages/qml_th.ts \
    languages/qml_ko.ts \
    languages/qml_hu.ts \
    languages/qml_nl.ts \
    languages/qml_sv.ts \
    languages/qml_tr.ts \
    languages/qml_uk.ts \
    languages/qml_zh_TW.ts \
    languages/qml_el.ts \
    languages/qml_hi.ts \
    languages/qml_it.ts \
    languages/qml_pt.ts \
    languages/qml_pt_BR.ts \
    languages/qml_pl.ts \
    languages/qml_cs.ts \
    languages/qml_he.ts \
    languages/qml_ckb.ts \
    languages/qml_lt.ts \
    languages/qml_et.ts \
    languages/qml_bg.ts \
    languages/qml_eo.ts \
    languages/qml_ta.ts

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../moonlight-common-c/release/ -lmoonlight-common-c
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../moonlight-common-c/debug/ -lmoonlight-common-c
else:unix: LIBS += -L$$OUT_PWD/../moonlight-common-c/ -lmoonlight-common-c

INCLUDEPATH += $$PWD/../moonlight-common-c/moonlight-common-c/src
DEPENDPATH += $$PWD/../moonlight-common-c/moonlight-common-c/src

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../qmdnsengine/release/ -lqmdnsengine
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../qmdnsengine/debug/ -lqmdnsengine
else:unix: LIBS += -L$$OUT_PWD/../qmdnsengine/ -lqmdnsengine

INCLUDEPATH += $$PWD/../qmdnsengine/qmdnsengine/src/include $$PWD/../qmdnsengine
DEPENDPATH += $$PWD/../qmdnsengine/qmdnsengine/src/include $$PWD/../qmdnsengine

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../h264bitstream/release/ -lh264bitstream
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../h264bitstream/debug/ -lh264bitstream
else:unix: LIBS += -L$$OUT_PWD/../h264bitstream/ -lh264bitstream

INCLUDEPATH += $$PWD/../h264bitstream/h264bitstream
DEPENDPATH += $$PWD/../h264bitstream/h264bitstream

!winrt {
    win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../AntiHooking/release/ -lAntiHooking
    else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../AntiHooking/debug/ -lAntiHooking

    INCLUDEPATH += $$PWD/../AntiHooking
    DEPENDPATH += $$PWD/../AntiHooking
}

unix:!macx: {
    isEmpty(PREFIX) {
        PREFIX = /usr/local
    }
    isEmpty(BINDIR) {
        BINDIR = bin
    }
    isEmpty(DATADIR) {
        DATADIR = share
    }

    target.path = $$PREFIX/$$BINDIR/

    desktop.files = deploy/linux/com.starlight.Starlight.desktop
    desktop.path = $$PREFIX/$$DATADIR/applications/

    for(size, $$list(16 24 32 48 64 128 256 512)) {
        icon$${size}.files = deploy/linux/icons/hicolor/$${size}x$${size}/apps/starlight.png
        icon$${size}.path = $$PREFIX/$$DATADIR/icons/hicolor/$${size}x$${size}/apps/
        INSTALLS += icon$${size}
    }

    appstream.files = deploy/linux/com.starlight.Starlight.appdata.xml
    appstream.path = $$PREFIX/$$DATADIR/metainfo/

    INSTALLS += target desktop appstream

    # Preserve the checked-in notices in Linux/Flatpak installations. Separate
    # component directories prevent identically named LICENSE files colliding.
    license_moonwalk.files = $$PWD/../LICENSE
    license_lucide.files = $$PWD/res/lucide/LICENSE
    license_moonlight.files = $$PWD/../moonlight-common-c/moonlight-common-c/LICENSE.txt
    license_enet.files = $$PWD/../moonlight-common-c/moonlight-common-c/enet/LICENSE
    license_nanors.files = $$PWD/../moonlight-common-c/moonlight-common-c/nanors/LICENSE
    license_qmdnsengine.files = $$PWD/../qmdnsengine/qmdnsengine/LICENSE.txt
    license_h264bitstream.files = $$PWD/../h264bitstream/h264bitstream/LICENSE
    license_controllerdb.files = $$PWD/SDL_GameControllerDB/LICENSE
    for(component, $$list(moonwalk lucide moonlight enet nanors qmdnsengine h264bitstream controllerdb)) {
        license_$${component}.path = $$PREFIX/$$DATADIR/licenses/moonwalk/$${component}/
        INSTALLS += license_$${component}
    }
}
win32 {
    RC_ICONS = starlight.ico
    QMAKE_TARGET_COMPANY = Moonwalk
    QMAKE_TARGET_DESCRIPTION = Moonwalk Remote Desktop Client
    QMAKE_TARGET_PRODUCT = Moonwalk

    CONFIG -= embed_manifest_exe
    QMAKE_LFLAGS += /MANIFEST:embed /MANIFESTINPUT:$${PWD}/Starlight.exe.manifest
}
macx {
    # Create Info.plist in object dir with the correct version string
    system(cp $$PWD/Info.plist $$OUT_PWD/Info.plist)
    system(sed -i -e 's/VERSION/$$cat(version.txt)/g' $$OUT_PWD/Info.plist)

    QMAKE_INFO_PLIST = $$OUT_PWD/Info.plist

    APP_BUNDLE_RESOURCES.files = starlight.icns
    APP_BUNDLE_RESOURCES.path = Contents/Resources

    APP_BUNDLE_PLIST.files = $$OUT_PWD/Info.plist
    APP_BUNDLE_PLIST.path = Contents

    QMAKE_BUNDLE_DATA += APP_BUNDLE_RESOURCES APP_BUNDLE_PLIST

    !disable-prebuilts {
        APP_BUNDLE_FRAMEWORKS.files = $$files(../libs/mac/Frameworks/*.framework, true) $$files(../libs/mac/lib/*.dylib, true)
        APP_BUNDLE_FRAMEWORKS.path = Contents/Frameworks

        QMAKE_BUNDLE_DATA += APP_BUNDLE_FRAMEWORKS

        QMAKE_RPATHDIR += @executable_path/../Frameworks
    }
}

# Starlight's own product version (independent of the upstream moonlight-qt
# engine version below). commandlineparser.cpp's --version prints this as the
# headline, plus the engine version underneath from VERSION_STR.
VERSION = 0.3.1
# The DISPLAYED version carries the alpha tag (owner ruling 2026-07-19 --
# sidebar link, settings footer, --version all say "0.3.1-alpha.17", independent of
# Skylight's SKYLIGHT_VERSION). VERSION itself stays bare: qmake feeds it to
# platform packaging fields that expect numeric triplets.
DEFINES += STARLIGHT_VERSION_STR=\\\"$$VERSION-alpha.17\\\"

# The upstream moonlight-qt engine version this fork tracks. Deliberately kept
# reading version.txt (unchanged) rather than following the VERSION bump above
# -- autoupdatechecker.cpp compares this against upstream moonlight-qt
# releases, and commandlineparser.cpp's --version prints it as the engine line.
DEFINES += VERSION_STR=\\\"$$cat(version.txt)\\\"

SOURCES += supportinfo.cpp
HEADERS += supportinfo.h
# Source archives/packagers can supply a revision explicitly; normal git builds identify themselves.
STARLIGHT_BUILD_REVISION = $$(STARLIGHT_BUILD_REVISION)
isEmpty(STARLIGHT_BUILD_REVISION) {
    STARLIGHT_BUILD_REVISION = $$system(git -C $$shell_quote($$PWD) describe --always --dirty 2>/dev/null)
}
isEmpty(STARLIGHT_BUILD_REVISION): STARLIGHT_BUILD_REVISION = unknown
# A changing compiler DEFINE does not invalidate existing Make objects. Keep
# identity in a generated include so incremental builds rebuild its consumers.
starlight_buildinfo.input = $$PWD/buildinfo.h.in
starlight_buildinfo.output = $$OUT_PWD/starlight_generated_buildinfo.h
QMAKE_SUBSTITUTES += starlight_buildinfo
INCLUDEPATH += $$OUT_PWD
HEADERS += buildinfo.h $$starlight_buildinfo.output
DISTFILES += buildinfo.h.in
