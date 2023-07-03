# Contributor: Luxzi <luxzi@pm.me>

pkgname=ragnarwm
_pkgname="Ragnar"
pkgver='1.3'
pkgrel=1
pkgdesc="Minimal, flexible & user-friendly X tiling window manager"
arch=('x86_64')
url="https://github.com/cococry/Ragnar"
license=('GPL')
groups=()
depends=()
makedepends=('git' 'make' 'gcc')
provides=('ragnarwm')
source=("${_pkgname}::git+https://github.com/cococry/${_pkgname}.git")
sha256sums=('SKIP')

pkgver() {
    cd $_pkgname
    echo $pkgver
}

build() {
    cd $_pkgname
    make ragnar
}

package() {
    cd $_pkgname 
    install -D -m777 ./ragnar "$pkgdir/usr/bin/ragnar"
    install -D -m777 ./ragnar.desktop "$pkgdir/usr/share/applications/ragnar.desktop"
    install -D -m777 ./ragnarstart "$pkgdir/usr/bin/ragnarstart"
}
