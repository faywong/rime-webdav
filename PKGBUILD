# Maintainer: Fay Wong <i@faywong.cc>
pkgname=fcitx5-rime-webdav
pkgver=5.1.19
pkgrel=1
pkgdesc="Fcitx5 plugin for Rime user dictionary WebDAV multi-device sync"
arch=('x86_64')
url="https://github.com/faywong/fcitx5-rime-webdav"
license=('GPL3')
groups=('fcitx5-im')
depends=(
  'fcitx5'
  'librime'
  'libcurl.so'
  'pugixml'
)
makedepends=(
  'cmake'
  'extra-cmake-modules'
  'gcc'
  'git'
  'pkgconf'
)
optdepends=(
  'fcitx5-rime: Rime input method engine for fcitx5'
  'fcitx5-configtool: GUI configuration tool'
)
source=("$pkgname::git+https://github.com/faywong/$pkgname.git")
sha256sums=('SKIP')

pkgver() {
  cd "$srcdir/$pkgname"
  _ver=$(sed -n 's/^Version=//p' rime-webdav/rime-webdav.conf)
  printf "%s" "${_ver:-5.1.19}"
}

build() {
  cd "$srcdir/$pkgname"
  cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_INSTALL_LIBDIR=/usr/lib \
    -DCMAKE_INSTALL_SYSCONFDIR=/etc
  cmake --build build -j"$(nproc)"
}

package() {
  cd "$srcdir/$pkgname"
  install -Dm755 "build/rime-webdav/libfcitx-rime-webdav.so" \
    "${pkgdir}/usr/lib/fcitx5/libfcitx-rime-webdav.so"

  install -Dm644 "rime-webdav/rime-webdav.conf" \
    "${pkgdir}/usr/share/fcitx5/addon/rime-webdav.conf"
}
