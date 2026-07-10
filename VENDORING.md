# Vendoring anylmdb

anylmdb is designed to be copied into other projects and compiled directly —
no build-time code generation, no include paths, no per-file compiler flags.

## The file set

`make vendor-list` prints the canonical list. It is flat (one directory) and
consists of:

- 8 wrapper sources to **compile**: `anylmdb_mdb09.c`, `anylmdb_midl09.c`,
  `anylmdb_mdb10.c`, `anylmdb_midl10.c`, `anylmdb_glue09.c`,
  `anylmdb_glue10.c`, `anylmdb_core.c`, `anylmdb_sniff.c`
- headers they include: `lmdb.h`, `midl.h` (stream-selecting shims),
  `anylmdb.h`, `anylmdb_ops.h`, `anylmdb_int.h`, the generated
  `anylmdb_rename09.h` / `anylmdb_rename10.h`, and the pristine upstream
  headers `lmdb_lmdb09.h`, `midl_lmdb09.h`, `lmdb_lmdb10.h`, `midl_lmdb10.h`
- the pristine upstream C sources, stored with a `.c.h` suffix:
  `mdb_lmdb09.c.h`, `midl_lmdb09.c.h`, `mdb_lmdb10.c.h`, `midl_lmdb10.c.h`
- license files: `LICENSE`, `LICENSE.lmdb09`, `COPYRIGHT.lmdb09`,
  `LICENSE.lmdb10`, `COPYRIGHT.lmdb10`

Compile the 8 `.c` files with `-pthread` on a 64-bit POSIX target; include
`lmdb.h` (full LMDB 1.0 API surface) and `anylmdb.h`. That is all.

## Why `.c.h`?

The upstream `mdb.c`/`midl.c` must not be compiled directly — each
`anylmdb_*.c` wrapper `#include`s them after the symbol-rename header. The
`.c.h` suffix keeps them out of build systems that compile every `*.c` they
see (cgo), while tools that filter by extension (like `go mod vendor`, which
keeps `.h`) retain them.

## Go (cgo)

Put the file set in a package directory. cgo compiles the 8 `anylmdb_*.c`
files automatically and ignores the `.c.h` files; `#include "lmdb.h"`
resolves within the package directory, so no `#cgo CFLAGS: -I...` is needed
(only `-pthread` on some toolchains). `go mod vendor` retains every file in
the set — verified with a consumer module.

## Python (setuptools) and other explicit build systems

List the 8 `anylmdb_*.c` files as sources and the package directory as an
include dir. Nothing else is required.

## Staying pristine

The `.c.h`/`.h` upstream files are byte-identical to their upstream release
(`update-lmdb.sh` verifies this with `cmp` at vendoring time). Do not patch
them; all adaptation lives in the wrapper files.
