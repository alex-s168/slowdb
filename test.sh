set -e
expect()
{
    RES="$($1)"
    if [[ "$RES" != "$2" ]]; then
        echo not expected
        exit 1
    fi
}

[[ -f test.db ]] && rm test.db

./cli.exe create test.db

./cli.exe put    test.db "a" "1"
./cli.exe put    test.db "b" "2"
./cli.exe put    test.db "c" "3"
./cli.exe put    test.db "d" "4"

expect "./cli.exe get test.db a" "1"
expect "./cli.exe get test.db c" "3"

./cli.exe put    test.db "a" "5"

expect "./cli.exe get test.db a" "5"
expect "./cli.exe get test.db c" "3"


