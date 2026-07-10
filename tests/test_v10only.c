/* 1.0-only API on a 0.9 environment returns ENOTSUP; the same calls reach
 * the engine on a 1.0 environment. Module helpers behave as documented. */
#include "anytest.h"

/* Deterministic toy checksum so 1.0's page verification passes. */
static void
dummy_sum(const MDB_val *src, MDB_val *dst, const MDB_val *key)
{
    const unsigned char *p = src->mv_data;
    unsigned sum = 0;
    (void)key;
    for (size_t i = 0; i < src->mv_size; i++)
        sum = sum * 31 + p[i];
    memcpy(dst->mv_data, &sum, dst->mv_size < 4 ? dst->mv_size : 4);
}

int
main(int argc, char **argv)
{
    at_init(argc, argv);

    /* post-open 1.0-only calls on a 0.9 env */
    const char *d09 = at_dir("e09");
    MDB_env *env = at_env_open(d09, ANYLMDB_VER_09, 0, 0);
    MDB_txn *txn;
    unsigned flags;

    CHECK_RC(mdb_env_rollback(env, 1), ENOTSUP);
    CHECK_RC(mdb_env_incr_dump(env, "/nonexistent", 1), ENOTSUP);
    CHECK_RC(mdb_env_incr_dumpfd(env, 1, 1), ENOTSUP);
    CHECK_RC(mdb_env_incr_loadfd(env, 1), ENOTSUP);
    CHECK_RC(mdb_env_set_pagesize(env, 8192), ENOTSUP); /* post-open path */

    CHECK_OK(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));
    CHECK_RC(mdb_txn_flags(txn, &flags), ENOTSUP);
    CHECK_RC(mdb_txn_prepare(txn), ENOTSUP);
    MDB_dbi dbi;
    CHECK_OK(mdb_dbi_open(txn, NULL, 0, &dbi));
    MDB_cursor *cur;
    CHECK_OK(mdb_cursor_open(txn, dbi, &cur));
    CHECK_RC(mdb_cursor_is_db(cur), ENOTSUP);
    mdb_cursor_close(cur);
    mdb_txn_abort(txn);
    mdb_env_close(env);

    /* pre-open 1.0-only settings + 0.9 selection = open fails with ENOTSUP */
    const char *dmix = at_dir("mix");
    CHECK_OK(mdb_env_create(&env));
    CHECK_OK(anylmdb_env_set_version(env, ANYLMDB_VER_09));
    CHECK_OK(mdb_env_set_pagesize(env, 8192)); /* buffered */
    CHECK_RC(mdb_env_open(env, dmix, 0, 0644), ENOTSUP);
    mdb_env_close(env);

    /* same calls work on a 1.0 env */
    const char *d10 = at_dir("e10");
    CHECK_OK(mdb_env_create(&env));
    CHECK_OK(anylmdb_env_set_version(env, ANYLMDB_VER_10));
    CHECK_OK(mdb_env_set_pagesize(env, 8192));
    CHECK_OK(mdb_env_set_checksum(env, dummy_sum, 4));
    CHECK_OK(mdb_env_open(env, d10, 0, 0644));

    CHECK_OK(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));
    CHECK_OK(mdb_txn_flags(txn, &flags));
    CHECK((flags & MDB_RDONLY) != 0);
    MDB_dbi dbi10;
    CHECK_OK(mdb_dbi_open(txn, NULL, 0, &dbi10));
    CHECK_OK(mdb_cursor_open(txn, dbi10, &cur));
    int rc = mdb_cursor_is_db(cur);
    CHECK(rc == 0 || rc == 1); /* reached the engine, not ENOTSUP */
    mdb_cursor_close(cur);
    mdb_txn_abort(txn);

    MDB_stat st;
    CHECK_OK(mdb_env_stat(env, &st));
    CHECK(st.ms_psize == 8192); /* pagesize actually applied */
    mdb_env_close(env);

    /* module helpers */
    char *errmsg = NULL;
    MDB_crypto_funcs *mcf = NULL;
    CHECK(mdb_modload("/nonexistent.so", NULL, &mcf, &errmsg) == NULL);
    CHECK(errmsg != NULL && strstr(errmsg, "anylmdb") != NULL);
    mdb_modunload(NULL); /* no-op, must not crash */
    return 0;
}
