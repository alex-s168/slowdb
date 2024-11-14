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

slowdb create test.db

slowdb put test.db "a" "1"
slowdb put test.db "b" "2"
slowdb put test.db "c" "3"
slowdb put test.db "d" "4"

expect "slowdb get test.db a" "1"
expect "slowdb get test.db c" "3"

slowdb put test.db "a" "5"

expect "slowdb get test.db a" "5"
expect "slowdb get test.db c" "3"


