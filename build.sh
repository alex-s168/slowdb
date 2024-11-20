set -e

if [ -z $CC ]; then
    if hash clang 2>/dev/null; then 
        : "${CC:=clang}"

        if hash lldb 2>/dev/null; then
            : "${CFLAGS:=-g -glldb}"
        else 
            : "${CFLAGS:=-g -ggdb}"
        fi
    elif hash cc 2>/dev/null; then
        : "${CC:=cc}"
        : "${CFLAGS:=}"
    elif hash gcc 2>/dev/null; then
        : "${CC:=gcc}"
        : "${CFLAGS:=-g -ggdb}"
    elif hash tcc 2>/dev/null; then
        : "${CC:=tcc}"
        : "${CFLAGS:=-g}"
    else 
        echo no c compiler found 
        echo manually set CC!
    fi
else 
    : "${CFLAGS:=}"
fi

has_include () {
    echo "#include <$1>" > .check.c
    if $CC .check.c -c -o .check.o 2>/dev/null; then
        echo 1
    else 
        echo 0
    fi
}

get_config () {
    cat config.h | awk '{if(substr($0,1,1) == "#" && $2 == "'"$1"'"){ print $3 }}'
}

if ! [ -f config.default.h ]; then
    echo "# generating default config"
    echo "// DO NOT EDIT (auto-generated)" > config.default.h
    echo "#define _SLOWDB_WITH_LZ4 $(has_include lz4.h)" >> config.default.h 
    echo "#define _SLOWDB_WITH_UNISHOX2 0" >> config.default.h 
fi

if ! [ -f config.h ]; then
    echo "# WARNING: you might want to set _SLOWDB_WITH_UNISHOX2 to 1 in the config.h, but remember that that changes the licence of the database from MIT to Apache 2.0"
    cp config.default.h config.h
fi

mkdir -p build

$CC $CFLAGS -c -o build/access.o src/access.c
$CC $CFLAGS -c -o build/iter.o src/iter.c
$CC $CFLAGS -c -o build/main.o src/main.c
$CC $CFLAGS -c -o build/utils.o src/utils.c
$CC $CFLAGS -c -o build/compress.o src/compress.c \
    -Wno-constant-conversion -Wno-pointer-sign -Wno-dangling-else

if [ "$(get_config _SLOWDB_WITH_UNISHOX2)" = "1" ]; then
    $CC -c -o build/unishox2.o src/unishox2/unishox2.c
fi

ar rcs build/slowdb.a build/*.o

libs=""
if [ "$(get_config _SLOWDB_WITH_LZ4)" = "1" ]; then
    libs+=" -llz4"
fi

$CC $CFLAGS $libs cli.c build/slowdb.a -o slowdb.exe
