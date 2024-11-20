set -e

: "${CC:=clang}"

mkdir -p build

$CC -c -o build/access.o src/access.c
$CC -c -o build/iter.o src/iter.c
$CC -c -o build/main.o src/main.c
$CC -c -o build/utils.o src/utils.c
$CC -c -o build/compress.o src/compress.c

# TODO: CHECK CONFIG
$CC -c -o build/unishox2.o src/unishox2/unishox2.c

ar rcs build/slowdb.a build/*.o

$CC cli.c build/slowdb.a -o slowdb.exe
