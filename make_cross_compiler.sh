
#!/bin/bash
set -e
# using : https://wiki.osdev.org/Cross-Compiler_Successful_Builds
echo "creating wingOS_cross Compiler"

download_and_extract() {
        echo "Downloading $2"
        wget -c "$1"
        echo "Extracting $2"
        tar xf "$2"
}



bin_utils_version="2.34"
binutils_file="binutils-$bin_utils_version.tar.xz"

gcc_version="10.1.0"
gcc_file="gcc-$gcc_version.tar.xz"

export PREFIX="$PWD/cross_compiler"
export TARGET="x86_64-pc-elf"
export PATH="$PREFIX/bin:$PATH"

mkdir -p ./cross_compiler/build/gcc ./cross_compiler/build/binutils
mkdir -p ./cross_compiler/src

cd ./cross_compiler/src

echo "downloading bin_utils"
download_and_extract "https://ftp.gnu.org/gnu/binutils/$binutils_file" "$binutils_file"
export binutils_src="$PWD/binutils-$bin_utils_version"


echo "downloading gcc"
download_and_extract "ftp://ftp.gnu.org/gnu/gcc/gcc-$gcc_version/$gcc_file" "$gcc_file"
export gcc_src="$PWD/gcc-$gcc_version"



cd ..
cd build

echo "building bin_utils"
cd binutils
"$binutils_src/configure" --target="$TARGET" 	\
        --prefix="$PREFIX" 	\
        --with-sysroot 		\
        --disable-nls 		\
        --disable-werror

make -j11
make install -j11
cd ..

echo "building gcc"
cd gcc
"$gcc_src/configure" --target="$TARGET" \
        --prefix="$PREFIX" 		\
        --disable-nls			\
        --enable-languages=c,c++	\
        --with-newlib

make all-gcc -j11
make all-target-libgcc -j11
make install-gcc -j11
make install-target-libgcc -j11

cd ..
