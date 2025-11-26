# Maintainer: FoxMoss <foxmoss@mediaology.com>
pkgname=dote-wm
pkgver=0.0.1
pkgrel=1
pkgdesc="A window manager framework for web technology"
arch=('x86_64')
url="https://github.com/FoxMoss/DoteWM/"
license=('BSD')
depends=(
    'gcc-libs'
    'libx11'
    'libxext'
    'libxcomposite'
    'libxfixes'
    'libxi'
    'mesa'           # Provides libGL, libGLU
    'glew'
    'nanomsg'
    'protobuf'
    'abseil-cpp'
)
makedepends=(
    'cmake'
    'git'
    'python'
)
source=("$pkgname-$pkgver.tar.gz::https://github.com/FoxMoss/DoteWM/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('dde9ff76478fe18d87bd7fd197de1e51dba3d11787393ef7a6b8bc1fa68be33f')

build() {
    cmake -B build -S "DoteWM-$pkgver" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build
}

package() {
    DESTDIR="$pkgdir" cmake --install build
}
