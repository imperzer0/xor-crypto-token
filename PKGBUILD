pkgname="xor-crypto-token"
epoch=1
pkgver=11
pkgrel=1
pkgdesc="token manager for xor encryptor program"
arch=("x86_64")
url="https://github.com/imperzer0/xor-crypto-token"
license=('GPL')
depends=("gcc-libs" "glibc" "parted" "procps-ng")
makedepends=("cmake>=3.0" "gcc" "parse-arguments>=1:8-1" "fish-completions>=1:8-2")
source=("local://main.cpp" "local://CMakeLists.txt" "local://terminal_output.hpp" "local://debug_construct.h")
md5sums=("SKIP" "SKIP" "SKIP" "SKIP" "SKIP")
install=xor-crypto-token.install

build()
{
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ .
	make
}

package()
{
	install -Dm755 "./$pkgname" "$pkgdir/usr/bin/$pkgname"
}
