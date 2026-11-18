# Building from source

Most people should use the Flatpak from [Releases](https://github.com/joaodriessen/moonwalk/releases). This page is for building Moonwalk yourself. If you plan to change the code, see the [development workflow](development.md) as well.

## Build the Flatpak

You'll need `flatpak` and `flatpak-builder`.

```bash
flatpak install flathub org.kde.Platform//6.10 org.kde.Sdk//6.10
flatpak-builder --user --install --force-clean --install-deps-from=flathub \
  build-dir flatpak/com.starlight.Starlight.yaml
flatpak run com.starlight.Starlight
```

The recipe is `flatpak/com.starlight.Starlight.yaml`. It downloads an exact commit from this repository, which can be older than both `main` and your checkout. This command builds that pinned revision, **not your local edits**. For a candidate from a specific committed checkout, use the [exact-source staging helper](development.md#local-flatpak-candidates). While the repository is private, anonymous fetching of the pinned source will fail; staging uses your existing checkout without placing credentials in the manifest.

## Build natively

Moonwalk builds the same way as Moonlight Qt, with Qt 6 and qmake, plus some bundled helper libraries that come along when you clone with `--recursive`.

**Debian and Ubuntu** are what Moonwalk has recently been built and tested on:

```bash
sudo apt-get update
sudo apt-get install -y qmake6 qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools \
  libqt6svg6-dev qt6-tools-dev-tools libgl1-mesa-dev \
  libsdl2-dev libsdl2-ttf-dev libopus-dev libssl-dev \
  libavcodec-dev libavformat-dev libswscale-dev libva-dev \
  libvdpau-dev libdrm-dev wayland-protocols libwayland-dev
```

**Fedora** should work, but it hasn't been verified recently:

```bash
sudo dnf install -y qt6-qtbase-devel qt6-qtdeclarative-devel \
  qt6-qtsvg-devel qt6-qttools-devel mesa-libGL-devel \
  SDL2-devel SDL2_ttf-devel opus-devel openssl-devel \
  ffmpeg-devel libva-devel libvdpau-devel libdrm-devel \
  wayland-protocols-devel wayland-devel
```

**Arch** should also work, and hasn't been build-verified either:

```bash
sudo pacman -S --needed qt6-base qt6-declarative qt6-svg qt6-tools \
  mesa sdl2 sdl2_ttf opus openssl ffmpeg libva libvdpau libdrm \
  wayland-protocols wayland
```

Then clone and build:

```bash
git clone --recursive https://github.com/joaodriessen/moonwalk.git
cd moonwalk
qmake6 moonlight-qt.pro
make -j$(nproc)
```

The program is `app/starlight`. To install it for everyone on the computer, along with its launcher entry and icon:

```bash
sudo make install   # installs to /usr/local/bin/starlight by default
```

### A native-looking window on GNOME

On KDE, Moonwalk's window gets a proper title bar and shadow automatically. On GNOME, a native build also needs the [QAdwaitaDecorations](https://github.com/FedoraQt/QAdwaitaDecorations) plugin, which isn't installed by default anywhere. The Flatpak includes it. Without it, the window works, but has a plain, flat frame.
