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
depends=(
  'xcb-util'
  'xcb-proto'
  'xcb-util-keysyms'
  'xcb-util-cursor'
  'xcb-util-wm'
  'libconfig'
  'xorg-server'
  'xorg-xinit'
  'mesa'
)
makedepends=('git' 'make' 'gcc')
optdepends=(
  'alacritty: default terminal keybind'
  'polybar: optional status/desktops bars'
  'ttf-dejavu: fonts for both the above'
)
provides=('ragnarwm')
options=('!debug')
backup=('etc/ragnarwm/ragnar.cfg')
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
