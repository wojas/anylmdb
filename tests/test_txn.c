/* Transaction and cursor-shell semantics on both engines: nested write
 * txns, reset/renew, wrapper identity getters, write-cursor auto-free at
 * commit, read-cursor close before txn end, renew across txns.
 * Closing a read cursor AFTER txn end is exercised on 0.9 only — on 1.0
 * that is upstream-undefined behavior which anylmdb passes through. */
#include "anytest.h"

static void
run(anylmdb_ver want)
{
    const char *dir = at_dir(want == ANYLMDB_VER_09 ? "e09" : "e10");
    MDB_env *env;
    MDB_txn *txn, *child;
    MDB_dbi dbi;
    MDB_val k, v;

    env = at_env_open(dir, want, 0, 0);

    /* nested write txns: child aborts, then child commits */
    CHECK_OK(mdb_txn_begin(env, NULL, 0, &txn));
    CHECK_OK(mdb_dbi_open(txn, NULL, 0, &dbi));
    k.mv_size = 1; k.mv_data = "a";
    v.mv_size = 1; v.mv_data = "1";
    CHECK_OK(mdb_put(txn, dbi, &k, &v, 0));

    CHECK_OK(mdb_txn_begin(env, txn, 0, &child));
    CHECK(mdb_txn_env(child) == env);
    k.mv_data = "b";
    CHECK_OK(mdb_put(child, dbi, &k, &v, 0));
    mdb_txn_abort(child);

    CHECK_OK(mdb_txn_begin(env, txn, 0, &child));
    k.mv_data = "c";
    CHECK_OK(mdb_put(child, dbi, &k, &v, 0));
    CHECK_OK(mdb_txn_commit(child));
    CHECK_OK(mdb_txn_commit(txn));

    CHECK_OK(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));
    k.mv_data = "a";
    CHECK_OK(mdb_get(txn, dbi, &k, &v));
    k.mv_data = "b";
    CHECK_RC(mdb_get(txn, dbi, &k, &v), MDB_NOTFOUND); /* child aborted */
    k.mv_data = "c";
    CHECK_OK(mdb_get(txn, dbi, &k, &v));               /* child committed */

    /* txn id and identity */
    CHECK(mdb_txn_env(txn) == env);
    CHECK(mdb_txn_id(txn) > 0);

    /* reset/renew */
    mdb_txn_reset(txn);
    CHECK_OK(mdb_txn_renew(txn));
    k.mv_data = "a";
    CHECK_OK(mdb_get(txn, dbi, &k, &v));
    mdb_txn_abort(txn);

    /* write-txn cursors: explicit close and auto-close at commit
     * (ASan verifies the shells don't leak) */
    CHECK_OK(mdb_txn_begin(env, NULL, 0, &txn));
    MDB_cursor *c1, *c2, *c3;
    CHECK_OK(mdb_cursor_open(txn, dbi, &c1));
    CHECK_OK(mdb_cursor_open(txn, dbi, &c2));
    CHECK_OK(mdb_cursor_open(txn, dbi, &c3));
    mdb_cursor_close(c2); /* unlink from the middle of the shell list */
    CHECK_OK(mdb_cursor_get(c1, &k, &v, MDB_FIRST));
    CHECK_OK(mdb_txn_commit(txn)); /* engine + wrapper free c1, c3 */

    /* read-only cursors: close before txn end; renew across txns */
    CHECK_OK(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));
    CHECK_OK(mdb_cursor_open(txn, dbi, &c1));
    CHECK_OK(mdb_cursor_open(txn, dbi, &c2));
    CHECK_OK(mdb_cursor_get(c1, &k, &v, MDB_FIRST));
    mdb_cursor_close(c1); /* before txn end: fine on both engines */
    mdb_txn_reset(txn);
    CHECK_OK(mdb_txn_renew(txn));
    CHECK_OK(mdb_cursor_renew(txn, c2));
    CHECK(mdb_cursor_txn(c2) == txn);
    CHECK_OK(mdb_cursor_get(c2, &k, &v, MDB_FIRST));
    mdb_cursor_close(c2);
    mdb_txn_abort(txn);

    /* renew a cursor into a brand-new read txn */
    CHECK_OK(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));
    CHECK_OK(mdb_cursor_open(txn, dbi, &c1));
    if (want == ANYLMDB_VER_09) {
        /* 0.9 contract: a read cursor may outlive its txn and be closed or
         * renewed afterwards. Passthrough must keep this working on 0.9. */
        mdb_txn_abort(txn);
        CHECK_OK(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));
        CHECK_OK(mdb_cursor_renew(txn, c1));
        CHECK_OK(mdb_cursor_get(c1, &k, &v, MDB_FIRST));
        mdb_txn_abort(txn);
        mdb_cursor_close(c1); /* after txn end: legal on 0.9 */
    } else {
        mdb_cursor_close(c1);
        mdb_txn_abort(txn);
    }

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
