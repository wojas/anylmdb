/* Format fidelity: databases written through the wrapper are byte-level
 * valid for the raw engines. Reads them back via the renamed mdb09_ and
 * mdb10_ symbols directly (visible in libanylmdb.a), bypassing the
 * wrapper entirely. */
#include "anytest.h"

/* Raw engine prototypes, spelled with public types: the handle types are
 * opaque pointers and the common-surface ABI is identical across streams. */
#define RAW_DECL(p) \
    extern int p##_env_create(MDB_env **); \
    extern int p##_env_open(MDB_env *, const char *, unsigned, mdb_mode_t); \
    extern void p##_env_close(MDB_env *); \
    extern int p##_txn_begin(MDB_env *, MDB_txn *, unsigned, MDB_txn **); \
    extern void p##_txn_abort(MDB_txn *); \
    extern int p##_dbi_open(MDB_txn *, const char *, unsigned, MDB_dbi *); \
    extern int p##_get(MDB_txn *, MDB_dbi, MDB_val *, MDB_val *);

RAW_DECL(mdb09)
RAW_DECL(mdb10)

#define RAW_READBACK(p, dir) do { \
    MDB_env *raw_env; \
    MDB_txn *raw_txn; \
    MDB_dbi raw_dbi; \
    MDB_val rk, rv; \
    CHECK_OK(p##_env_create(&raw_env)); \
    CHECK_OK(p##_env_open(raw_env, dir, MDB_RDONLY, 0644)); \
    CHECK_OK(p##_txn_begin(raw_env, NULL, MDB_RDONLY, &raw_txn)); \
    CHECK_OK(p##_dbi_open(raw_txn, NULL, 0, &raw_dbi)); \
    rk.mv_size = 5; rk.mv_data = "hello"; \
    CHECK_OK(p##_get(raw_txn, raw_dbi, &rk, &rv)); \
    CHECK(rv.mv_size == 5 && memcmp(rv.mv_data, "world", 5) == 0); \
    p##_txn_abort(raw_txn); \
    p##_env_close(raw_env); \
} while (0)

static const char *
write_via_wrapper(anylmdb_ver want)
{
    const char *dir = at_dir(want == ANYLMDB_VER_09 ? "e09" : "e10");
    MDB_env *env = at_env_open(dir, want, 0, 0);
    MDB_txn *txn;
    MDB_dbi dbi;
    MDB_val k, v;
    CHECK_OK(mdb_txn_begin(env, NULL, 0, &txn));
    CHECK_OK(mdb_dbi_open(txn, NULL, 0, &dbi));
    k.mv_size = 5; k.mv_data = "hello";
    v.mv_size = 5; v.mv_data = "world";
    CHECK_OK(mdb_put(txn, dbi, &k, &v, 0));
    CHECK_OK(mdb_txn_commit(txn));
    mdb_env_close(env);
    return dir;
}

int
main(int argc, char **argv)
{
    at_init(argc, argv);
    RAW_READBACK(mdb09, write_via_wrapper(ANYLMDB_VER_09));
    RAW_READBACK(mdb10, write_via_wrapper(ANYLMDB_VER_10));
    return 0;
}
