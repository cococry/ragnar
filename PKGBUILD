# Contributor: Luxzi <luxzi@pm.me>
# Contributor: h8d13 <hadean-eon-dev@proton.me>

pkgname=ragnarwm
_pkgname="ragnar"
pkgver='2'
pkgrel=1
pkgdesc="Minimal, flexible & user-friendly X tiling window manager"
arch=('x86_64')
url="https://github.com/h8d13/ragnar"
license=('GPL')
groups=()
# mirrors the NEEDED entries of bin/ragnar, nothing more:
# libxcb covers xcb + randr + xfixes, libx11 covers X11 + X11-xcb.
depends=(
  'libxcb'
  'libx11'
  'xcb-util-keysyms'
  'xcb-util-cursor'
  'xcb-util-wm'
  'libconfig'
  'xorg-server'
  'xorg-xinit'
)
makedepends=('git' 'make' 'gcc')
optdepends=(
  'alacritty: default terminal keybind'
  'polybar: optional status/desktops bars'
  'ttf-dejavu: fonts for both the above'
  'wireplumber: default volume keybinds'
  'brightnessctl: default brightness keybinds'
  'playerctl: default media player keybinds'
)
provides=('ragnarwm')
options=('!debug')
backup=('etc/ragnarwm/ragnar.cfg')
# capabilities do not survive the package, so cap_sys_nice is applied by a
# scriptlet. see src/realtime.c for what it buys.
install="${pkgname}.install"
source=("${_pkgname}::git+${url}.git")
sha256sums=('SKIP')

pkgver() {
  cd $_pkgname || exit 1
  echo $pkgver
}

build() {
  cd $_pkgname || exit 1
  make
}

package() {
  cd $_pkgname || exit 1
  # single source of truth: same install target ./install.sh drives.
  # only the systemwide default cfg is installed, user cfg is a manual cp.
  # this is usually convention for most WM configs, template in /etc
  make DESTDIR="$pkgdir" PREFIX=/usr install
}
