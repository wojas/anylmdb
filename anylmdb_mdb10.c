/* Compiles the pristine LMDB 1.0 engine (mdb.c) with every extern symbol
 * renamed to mdb10_*. The upstream source is vendored byte-identical as
 * mdb_lmdb10.c.h; all adaptation happens here. 1.0's module.c/crypto.c are
 * deliberately not built (no dlopen dependency); the wrapper provides
 * mdb_modload/mdb_modunload/mdb_modsetup replacements.
 *
 * On Windows a patched variant is compiled instead: upstream 1.0.0 does
 * not compile there (LARGE_INTEGER union misuse in mdb_env_copyfd0 and
 * mdb_env_incr_loadfd). mdb_lmdb10_win.c.h is generated from the pristine
 * source plus the minimal checked-in mdb_lmdb10_win.patch — see
 * scripts/gen-win10.sh. POSIX builds always use the pristine source.
 */
#define ANYLMDB_STREAM 10
#include "anylmdb_rename10.h"
#ifdef _WIN32
#include "mdb_lmdb10_win.c.h"
#else
#include "mdb_lmdb10.c.h"
#endif
