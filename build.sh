set -e

if ! [ -z $1 ]; then
    if [ $1 = "clean" ]; then
        >&2 echo \# resetting config
        rm config.default.h
        rm config.h
    else
        >&2 echo invalid usage! expected either \"clean\" as argument, or no arguments at all
        exit 1
    fi
fi

if ! hash awk 2>/dev/null; then
    >&2 echo awk required
    exit 1
fi

DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

[ -d "$DIR/build" ] && rm -r "$DIR/build"
mkdir "$DIR/build"

cc_ok () {
    echo "int main(void) { return 0; }" > "$DIR/build/.check.c"
    if $CC "$DIR/build/.check.c" -o "$DIR/build/.check.exe" 2>/dev/null; then
        echo 1
    else 
        echo 0
    fi
}

has_include () {
    echo "#include <$1>" > "$DIR/build/.check.c"
    if $CC "$DIR/build/.check.c" "$2" -c -o "$DIR/build/.check.o" 2>/dev/null; then
        echo 1
    else 
        echo 0
    fi
}

get_config () {
    cat "$DIR/config.h" | awk '{if(substr($0,1,1) == "#" && $2 == "'"$1"'"){ print $3 }}'
}

OLD_CFLAGS="$CFLAGS"

if [ -z $CC ]; then if hash cc 2>/dev/null; then 
        CC=cc

	if [ "$(cc_ok $CC)" = "1" ]; then
		: "${CFLAGS:=}"
	else
		>&2 echo $CC cannot compile a simple program, trying next compiler
		unset CC
	fi
fi fi
if [ -z $CC ]; then if hash clang 2>/dev/null; then 
        CC=clang

	if [ "$(cc_ok $CC)" = "1" ]; then
		if hash lldb 2>/dev/null; then
		    : "${CFLAGS:=-g -glldb}"
		else 
		    : "${CFLAGS:=-g -ggdb}"
		fi
	else
		>&2 echo $CC cannot compile a simple program, trying next compiler
		unset CC
	fi
fi fi
if [ -z $CC ]; then if hash gcc 2>/dev/null; then 
        CC=gcc

	if [ "$(cc_ok $CC)" = "1" ]; then
		: "${CFLAGS:=-g -ggdb}"
	else
		>&2 echo $CC cannot compile a simple program, trying next compiler
		unset CC
	fi
fi fi
if [ -z $CC ]; then if hash tcc 2>/dev/null; then 
        CC=tcc

	if [ "$(cc_ok $CC)" = "1" ]; then
		: "${CFLAGS:=-g}"
		: "${AR:=tcc -ar}"
	else
		>&2 echo $CC cannot compile a simple program, trying next compiler
		unset CC
	fi
fi fi

if [ -z $CC ]; then
	>&2 echo no usable c compiler found 
	>&2 echo manually set CC!
	exit 1
fi

: "${CFLAGS:=}"
: "${AR:=ar}"

CFLAGS+=" "
CFLAGS+=$OLD_CFLAGS

>&2 echo "slowdb: CC=\"$CC\" AR=\"$AR\" CFLAGS=\"$CFLAGS\""

if ! [ -f "$DIR/config.default.h" ]; then
    >&2 echo "# generating default config"
    echo "// if this is config.default.h, DO NOT EDIT, AUTO GENERATED" > "$DIR/config.default.h"
    echo "#define _SLOWDB_WITH_LZ4 $(has_include lz4.h -llz4)" >> "$DIR/config.default.h" 
    echo "#define _SLOWDB_WITH_STRPACK  0" >> "$DIR/config.default.h"
    echo "#define _SLOWDB_WITH_UNISHOX2 0" >> "$DIR/config.default.h"
fi

if ! [ -f "$DIR/config.h" ]; then
    >&2 echo "# WARNING: you might want to set _SLOWDB_WITH_UNISHOX2 to 1 in the config.h, but remember that that changes the licence of the database from MIT to Apache 2.0"
    cp "$DIR/config.default.h" "$DIR/config.h"
fi

libs="$DIR/build/slowdb.a"
if [ "$(get_config _SLOWDB_WITH_LZ4)" = "1" ]; then
    libs+=" -llz4"
fi

$CC $CFLAGS -c -o "$DIR/build/access.o" "$DIR/src/access.c" 1>&2
$CC $CFLAGS -c -o "$DIR/build/iter.o" "$DIR/src/iter.c" 1>&2
$CC $CFLAGS -c -o "$DIR/build/main.o" "$DIR/src/main.c" 1>&2
$CC $CFLAGS -c -o "$DIR/build/utils.o" "$DIR/src/utils.c" 1>&2
$CC $CFLAGS -c -o "$DIR/build/compress.o" "$DIR/src/compress.c" \
    -Wno-constant-conversion -Wno-pointer-sign -Wno-dangling-else 1>&2

if [ "$(get_config _SLOWDB_WITH_UNISHOX2)" = "1" ]; then
    $CC $CFLAGS -c -o "$DIR/build/unishox2.o" "$DIR/src/unishox2/unishox2.c" 1>&2
fi

$AR rcs "$DIR/build/slowdb.a" "$DIR/build/"*.o 1>&2

$CC $CFLAGS "$DIR/cli.c" $libs -o "$DIR/slowdb.exe" 1>&2

echo "$libs"
