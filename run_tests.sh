source build.sh      # to get CC and CFLAGS
built=$(./build.sh)

for f in tests/*.c; do
    $CC $CFLAGS $built $f -o $f.exe
    $f.exe
done
