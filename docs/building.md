# Building Moonwalk

The Linux client uses Qt 6 and qmake. Clone with submodules before building.

```sh
git clone --recursive https://github.com/joaodriessen/moonwalk.git
cd moonwalk
```

If you already have a checkout, run `git submodule update --init --recursive`.
The private repository requires GitHub access.

## Flatpak

Install `flatpak`, `flatpak-builder`, and the KDE 6.10 platform and SDK. The
recipe uses Flathub for its runtime dependencies.

```sh
flatpak install --user flathub org.kde.Platform//6.10 org.kde.Sdk//6.10
```

The [manifest](../flatpak/com.starlight.Starlight.yaml) pins a Git commit. For
the private repository, use the [local candidate workflow](development.md#local-flatpak-candidates).
It stages a committed revision and its submodules from your authenticated
checkout. The helper prints the build and bundle commands. A manifest build
that fetches from GitHub needs access to the pinned commit.

After building a bundle, install it with `flatpak install --user ./NAME.flatpak`
and run `flatpak run com.starlight.Starlight`. Replace `NAME.flatpak` with
the bundle's filename. See the [install guide](guides/getting-started.md) for
updates and removal.

## Native Linux build

CI builds on Ubuntu 24.04 with Qt 6.4.2. The following packages include the
compiler and Qt Quick modules needed for a native build and offscreen checks.

```sh
sudo apt-get update
sudo apt-get install build-essential git pkg-config python3 python3-yaml \
  qmake6 qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools \
  libqt6svg6-dev qt6-tools-dev-tools libgl1-mesa-dev \
  qml6-module-qtcore qml6-module-qtqml qml6-module-qtqml-models \
  qml6-module-qtqml-workerscript qml6-module-qtquick qml6-module-qtquick-window \
  qml6-module-qtquick-layouts qml6-module-qtquick-templates \
  qml6-module-qtquick-controls qml6-module-qtquick-dialogs qml6-module-qttest \
  qml6-module-qt-labs-settings \
  libsdl2-dev libsdl2-ttf-dev libopus-dev libssl-dev \
  libavcodec-dev libavformat-dev libswscale-dev libva-dev \
  libvdpau-dev libdrm-dev wayland-protocols libwayland-dev
```

On other distributions, install the corresponding development packages.
Package names and FFmpeg availability vary by distribution.

- On Fedora, look for `qt6-qtbase-devel`, `qt6-qtdeclarative-devel`,
  `qt6-qtsvg-devel`, `qt6-qttools-devel`, `SDL2-devel`, `SDL2_ttf-devel`,
  and the FFmpeg, OpenSSL, Opus, VA-API, VDPAU, DRM, and Wayland development
  packages.
- On Arch, the equivalent packages include `base-devel`, `qt6-base`,
  `qt6-declarative`, `qt6-svg`, `qt6-tools`, `sdl2`, `sdl2_ttf`,
  `opus`, `openssl`, `ffmpeg`, `libva`, `libvdpau`, `libdrm`,
  `wayland-protocols`, and `wayland`.

Run `./dev build` for an incremental build. The command prints the executable
path. For a standalone qmake build, run the following from the checkout:

```sh
mkdir -p build
cd build
qmake6 ../moonlight-qt.pro
make -j"$(nproc)"
./app/starlight
```

To install the native binary, launcher, and icons under `/usr/local`, run
`sudo make install` from that build directory.

### GNOME window decorations

On Wayland, [QAdwaitaDecorations](https://github.com/FedoraQt/QAdwaitaDecorations)
provides GNOME-style decorations where the window uses Qt's Wayland
decoration plugin. The Flatpak includes it. On a native build without the
plugin, Qt can fall back to simpler decorations. Plasma uses its own style
when the required KDE modules are available.

[Development workflow](development.md) · [Release checklist](releasing.md)
