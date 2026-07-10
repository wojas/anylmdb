/* Basic data operations through the wrapper, run against both engines:
 * put/get/del, named DBs, dupsort, cursor iteration, stat, drop. */
#include "anytest.h"

static void
run(anylmdb_ver want)
{
    const char *dir = at_dir(want == ANYLMDB_VER_09 ? "e09" : "e10");
    MDB_env *env;
    MDB_txn *txn;
    MDB_dbi dbi, dup;
    MDB_val k, v;
    MDB_stat st;
    char kb[32], vb[32];

    CHECK_OK(mdb_env_create(&env));
    CHECK_OK(anylmdb_env_set_version(env, want));
    CHECK_OK(mdb_env_set_maxdbs(env, 4));
    CHECK_OK(mdb_env_open(env, dir, 0, 0644));

    /* named DB with 100 keys */
    CHECK_OK(mdb_txn_begin(env, NULL, 0, &txn));
    CHECK_OK(mdb_dbi_open(txn, "main", MDB_CREATE, &dbi));
    for (int i = 0; i < 100; i++) {
        k.mv_size = (size_t)snprintf(kb, sizeof kb, "key%03d", i);
        k.mv_data = kb;
        v.mv_size = (size_t)snprintf(vb, sizeof vb, "val%03d", i);
        v.mv_data = vb;
        CHECK_OK(mdb_put(txn, dbi, &k, &v, 0));
    }
    CHECK_OK(mdb_txn_commit(txn));

    /* read back, iterate with a cursor */
    CHECK_OK(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));
    k.mv_size = 6; k.mv_data = "key042";
    CHECK_OK(mdb_get(txn, dbi, &k, &v));
    CHECK(v.mv_size == 6 && memcmp(v.mv_data, "val042", 6) == 0);

    MDB_cursor *cur;
    CHECK_OK(mdb_cursor_open(txn, dbi, &cur));
    CHECK(mdb_cursor_dbi(cur) == dbi);
    CHECK(mdb_cursor_txn(cur) == txn);
    int n = 0;
    int rc = mdb_cursor_get(cur, &k, &v, MDB_FIRST);
    while (rc == MDB_SUCCESS) {
        n++;
        rc = mdb_cursor_get(cur, &k, &v, MDB_NEXT);
    }
    CHECK_RC(rc, MDB_NOTFOUND);
    CHECK(n == 100);
    k.mv_size = 6; k.mv_data = "key050";
    CHECK_OK(mdb_cursor_get(cur, &k, &v, MDB_SET_RANGE));
    CHECK(memcmp(v.mv_data, "val050", 6) == 0);
    mdb_cursor_close(cur);

    CHECK_OK(mdb_stat(txn, dbi, &st));
    CHECK(st.ms_entries == 100);
    mdb_txn_abort(txn);

    /* delete a key */
    CHECK_OK(mdb_txn_begin(env, NULL, 0, &txn));
    k.mv_size = 6; k.mv_data = "key042";
    CHECK_OK(mdb_del(txn, dbi, &k, NULL));
    CHECK_RC(mdb_get(txn, dbi, &k, &v), MDB_NOTFOUND);
    CHECK_OK(mdb_txn_commit(txn));

    /* dupsort DB */
    CHECK_OK(mdb_txn_begin(env, NULL, 0, &txn));
    CHECK_OK(mdb_dbi_open(txn, "dups", MDB_CREATE | MDB_DUPSORT, &dup));
    k.mv_size = 3; k.mv_data = "dup";
    for (int i = 0; i < 5; i++) {
        v.mv_size = (size_t)snprintf(vb, sizeof vb, "v%d", i);
        v.mv_data = vb;
        CHECK_OK(mdb_put(txn, dup, &k, &v, 0));
    }
    CHECK_OK(mdb_cursor_open(txn, dup, &cur));
    CHECK_OK(mdb_cursor_get(cur, &k, &v, MDB_SET));
    mdb_size_t count = 0;
    CHECK_OK(mdb_cursor_count(cur, &count));
    CHECK(count == 5);
    mdb_cursor_close(cur);
    /* drop it */
    CHECK_OK(mdb_drop(txn, dup, 1));
    CHECK_OK(mdb_txn_commit(txn));

    /* env-level introspection */
    CHECK_OK(mdb_env_stat(env, &st));
    CHECK(st.ms_psize > 0);
    MDB_envinfo info;
    CHECK_OK(mdb_env_info(env, &info));
    CHECK(info.me_mapsize > 0);
    CHECK(mdb_env_get_maxkeysize(env) >= 511);

    mdb_env_close(env);
}

int
main(int argc, char **argv)
{
    at_init(argc, argv);
    run(ANYLMDB_VER_09);
    run(ANYLMDB_VER_10);
    return 0;
}
