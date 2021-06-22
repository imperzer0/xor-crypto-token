pkgname="xor-crypto-token"
epoch=1
pkgver=10
pkgrel=2
pkgdesc="token manager for xor encryptor program"
arch=("x86_64")
url="https://github.com/imperzer0/xor-crypto-token"
license=('GPL')
# depends=()
makedepends=("cmake>=3.0")
source=("local://main.cpp" "local://CMakeLists.txt" "local://completions.hpp" "local://terminal_output.hpp" "local://debug_construct.h")
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
