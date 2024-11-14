# slowdb
A safe and simple header-only K-V store

(slowdb is called "slowdb" not because it is slow, but because it is not blazingly fast)

Key-Value storage in C:
- simple
- tiny
- hash table of keys to file offsets
- persistent: writes are instantly written to the disk
- NOT concurrent
- header-only
- MIT-licenced
