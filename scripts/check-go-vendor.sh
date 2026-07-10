#!/bin/sh
# Verify the Go-vendorability contract: a cgo package holding the anylmdb
# file set must survive `go mod vendor` in a consumer module (including the
# .c.h upstream sources) and build with cgo compiling only the anylmdb_*.c
# wrapper TUs.
set -eu

cd "$(dirname "$0")/.."
root=$(pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

mkdir -p "$tmp/anylmdbgo/anylmdb" "$tmp/consumer"
for f in $(make -s vendor-list); do
    cp "$root/$f" "$tmp/anylmdbgo/anylmdb/"
done

cat > "$tmp/anylmdbgo/anylmdb/anylmdb.go" <<'EOF'
// Package anylmdb is a minimal cgo binding used to verify vendorability.
package anylmdb

/*
#cgo CFLAGS: -pthread
#cgo LDFLAGS: -pthread
#include "anylmdb.h"
*/
import "C"

// Version returns the anylmdb version string.
func Version() string {
	return C.GoString(C.mdb_version(nil, nil, nil))
}
EOF
(cd "$tmp/anylmdbgo" && go mod init example.com/anylmdbgo >/dev/null 2>&1)

cat > "$tmp/consumer/main.go" <<'EOF'
package main

import (
	"fmt"

	"example.com/anylmdbgo/anylmdb"
)

func main() {
	fmt.Println(anylmdb.Version())
}
EOF
cd "$tmp/consumer"
go mod init example.com/consumer >/dev/null 2>&1
go mod edit -replace example.com/anylmdbgo=../anylmdbgo
go mod tidy >/dev/null 2>&1
go mod vendor

got=$(ls vendor/example.com/anylmdbgo/anylmdb/ | grep -c '\.c\.h$' || true)
if [ "$got" != 4 ]; then
    echo "FAIL: expected 4 vendored .c.h files, got $got" >&2
    exit 1
fi

go build -mod=vendor -o consumer .
out=$(./consumer)
case "$out" in
*ANYLMDB*"LMDB 1.0"*"LMDB 0.9"*)
    echo "check-go-vendor: OK ($out)" ;;
*)
    echo "FAIL: unexpected version output: $out" >&2
    exit 1 ;;
esac
