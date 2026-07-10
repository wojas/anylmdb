/* Drop-in smoke test: a plain liblmdb client (standard lmdb.h API only, no
 * anylmdb extensions) built against the liblmdb.so.1-soname drop-in build.
 * Verifies the loaded library is anylmdb and that basic operations work. */
#include <stdio.h>
#include <string.h>

#include "lmdb.h"

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

int
main(int argc, char **argv)
{
    CHECK(argc >= 2);

    int major = 0;
    char *ver = mdb_version(&major, NULL, NULL);
    CHECK(strstr(ver, "ANYLMDB") != NULL); /* proves anylmdb is loaded */
    CHECK(major == 1);

    MDB_env *env;
    MDB_txn *txn;
    MDB_dbi dbi;
    MDB_val k, v;
    CHECK(mdb_env_create(&env) == 0);
    CHECK(mdb_env_open(env, argv[1], 0, 0644) == 0);
    CHECK(mdb_txn_begin(env, NULL, 0, &txn) == 0);
    CHECK(mdb_dbi_open(txn, NULL, 0, &dbi) == 0);
    k.mv_size = 4; k.mv_data = "ping";
    v.mv_size = 4; v.mv_data = "pong";
    CHECK(mdb_put(txn, dbi, &k, &v, 0) == 0);
    CHECK(mdb_txn_commit(txn) == 0);
    CHECK(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn) == 0);
    CHECK(mdb_get(txn, dbi, &k, &v) == 0);
    CHECK(v.mv_size == 4 && memcmp(v.mv_data, "pong", 4) == 0);
    mdb_txn_abort(txn);
    mdb_env_close(env);
    return 0;
}
