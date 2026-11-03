# Maintainer: GUTeam
pkgname=colibry-taskmanager
pkgver=0.2.0
pkgrel=1
pkgdesc="Tasks for the Colibry"
arch=('x86_64' 'aarch64')
url="https://github.com/GreatUserTeam/$pkgname"
license=('MIT')
depends=('qt6-base')
makedepends=('cmake' 'ninja')
source=("$pkgname-$pkgver.tar.gz::$url/archive/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
    cmake -S "$pkgname-$pkgver" -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DColibry_BUILD_TESTS=OFF
    cmake --build build
}

package() {
    DESTDIR="$pkgdir" cmake --install build
}
