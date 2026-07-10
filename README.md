# anylmdb

One C library carrying **both LMDB 0.9 and LMDB 1.0**, exposing the standard
`mdb_*` API. Any program that links liblmdb can link anylmdb instead and
transparently open databases in either on-disk format.

## Why

LMDB 1.0.0 (June 2026) keeps the familiar `mdb_*` C API but its database
format is **mutually incompatible** with 0.9: a 1.0 build cannot open 0.9
files and vice versa (both fail at `mdb_env_open`). Every LMDB consumer that
meets databases of unknown vintage — language bindings, backup/sync tools,
long-lived services — faces the same migration problem. anylmdb solves it
once, at the C level, for all of them:

- Both engines are compiled into the library from **pristine upstream
  sources** (0.9.35 and 1.0.0), with every extern symbol mechanically
  prefixed (`mdb09_*` / `mdb10_*`) so they coexist in one binary.
- The public `mdb_*` entrypoints dispatch **per environment**: at
  `mdb_env_open()` the data file's meta page is inspected (without touching
  the lock file) and the matching engine drives that environment from then
  on.

## Build

```sh
make            # libanylmdb.a + libanylmdb.so / .dylib
make test       # test suite + symbol-hygiene check
make dropin     # additionally: liblmdb.so.0 / liblmdb.so.1 (see below)
make install    # PREFIX=/usr/local
```

CI-style container runs (Debian/glibc with ASan, Alpine/musl, plus the
Linux drop-in check):

```sh
make docker-test
```

64-bit POSIX only (Linux, macOS). `MDB_VL32` is not supported (`#error`).

Link with `-lanylmdb` and include the shipped `lmdb.h` (the pristine LMDB
1.0 header — the full API surface) and optionally `anylmdb.h`.

## Choosing the format

- **Existing databases**: always opened with the engine matching their
  on-disk format, detected by reading the data file's meta page. The
  detection never opens the environment — that would be unsafe, because a
  failed 1.0 open of a 0.9 file can rewrite the lock file first.
- **New databases** default to the **LMDB 0.9 format** (best interoperability
  while distributions still ship 0.9). Override, in order of precedence:
  1. `anylmdb_env_set_version(env, ANYLMDB_VER_10)` before `mdb_env_open`;
  2. `anylmdb_set_default_version(ANYLMDB_VER_10)` process-wide;
  3. environment variable `ANYLMDB_DEFAULT=10` (or `09`).
- Introspection: `anylmdb_env_get_version()` returns the engine in use;
  `anylmdb_sniff(path, flags, &ver)` detects a database's format without
  opening it.
- Files written by lmdb-js prerelease snapshots (data version 2) are
  incompatible with both release lines; anylmdb fails to open them with the
  dedicated error `ANYLMDB_FORMAT_UNSUPPORTED`.

LMDB **1.0-only functions** (`mdb_txn_prepare`, `mdb_env_set_encrypt`,
`mdb_env_rollback`, `mdb_env_incr_*`, ...) are all exported; called on a
0.9-format environment they return `ENOTSUP`. That makes `ENOTSUP` a clean
runtime feature probe.

## Drop-in replacement builds

`make dropin` produces shared libraries with liblmdb's usual sonames
(`liblmdb.so.0` and `liblmdb.so.1` on Linux), so existing binaries can be
repointed or `LD_PRELOAD`ed without a relink. On macOS the equivalent
`liblmdb.{0,1}.dylib` are built; interposition there is best-effort
(direct-link replacement works, `DYLD_INSERT_LIBRARIES` depends on the
binary).

## Caveats and deliberate behavior

- **`mdb_version()`** reports the wrapper: numerically 1.0.0 (matching the
  shipped header, so code gating 1.0 workarounds on `major >= 1` stays
  safe), with the string
  `"ANYLMDB x.y.z: LMDB 1.0.0 (...) + LMDB 0.9.35 (...)"`. This is the one
  deliberate deviation from drop-in behavior. Note that the string contains
  both `LMDB 1.0.0` and `LMDB 0.9.35` as substrings, so a naive
  `grep "LMDB $ver"` version check matches either.
- **Closing a read-only cursor after its transaction ended** is documented
  and legal in LMDB 0.9 but is a use-after-free in current LMDB 1.0
  (`mdb_cursor_close` touches the freed transaction when built with the
  default `MDB_RPAGE_CACHE`). anylmdb passes this through unchanged: on a
  0.9 environment it works, on a 1.0 environment it is the same undefined
  behavior you would get linking 1.0 directly. Portable code should close
  read-only cursors before ending their transaction.
- **No format conversion**: `mdb_env_copy`/`mdb_env_copyfd` write the
  environment's native format. Migrating 0.9 → 1.0 means dump/reload (which
  anylmdb makes possible in a single process: read with one env, write with
  another).
- **Named-database keys**: LMDB 1.0 stores DB names in the root database
  with a trailing NUL, 0.9 without. `mdb_dbi_open` is unaffected, but tools
  that scan the root database for names must tolerate both.
- **Lock files**: 0.9 and 1.0 processes can never share one environment
  concurrently; the engines themselves fail with `MDB_VERSION_MISMATCH`.
- **Crypto modules**: 1.0's dlopen-based `mdb_modload` is not included
  (also for license reasons: `module.c` is Symas dual-use licensed, not
  OpenLDAP-licensed). It returns NULL with an explanatory message.
  Statically provided hooks work: `mdb_modsetup`, `mdb_env_set_encrypt` and
  `mdb_env_set_checksum` are fully functional on 1.0 environments.
- Wrapper handles cost one small allocation per env/txn/cursor and one
  pointer indirection per call.

## Vendoring into other projects

The build is deliberately flat and codegen-free so language bindings can
compile anylmdb directly — see [VENDORING.md](VENDORING.md). `make
vendor-list` prints the file set.

When switching a binding from its own vendored LMDB to anylmdb, verify the
linkage with `nm`/`otool`/`ldd` on the resulting binary: a leftover vendored
`mdb.c` in the package silently wins over the shared library (every `mdb_*`
symbol resolves statically) and produces convincing but bogus results.

## Updating the vendored LMDB versions

```sh
./update-lmdb.sh 09 0.9.36    # hypothetical future releases
./update-lmdb.sh 10 1.0.1
make clean all check-symbols test
```

Sources download from the GitHub mirror (`github.com/LMDB/lmdb`) only.
Upstream files are stored byte-identical (`cmp`-verified); the symbol rename
headers are regenerated from `nm` output and checked in. `VERSIONS` records
what is vendored.

## License

OpenLDAP Public License 2.8 — both for the vendored LMDB sources (see
`LICENSE.lmdb09` / `LICENSE.lmdb10`, `COPYRIGHT.lmdb09` /
`COPYRIGHT.lmdb10`) and for the anylmdb wrapper code itself (`LICENSE`).
