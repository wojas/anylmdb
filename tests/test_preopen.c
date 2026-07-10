/* Pre-open behavior: buffered settings visible via getters and effective
 * after open; maxkeysize special cases; userctx round-trip. */
#include "anytest.h"

int
main(int argc, char **argv)
{
    at_init(argc, argv);

    /* NULL/unopened maxkeysize must not crash (1.0 segfaults on NULL) */
    CHECK(mdb_env_get_maxkeysize(NULL) == 511);

    MDB_env *env;
    CHECK_OK(mdb_env_create(&env));
    CHECK(mdb_env_get_maxkeysize(env) == 511);

    /* buffered settings readable back before open */
    CHECK_OK(mdb_env_set_mapsize(env, 4u << 20));
    CHECK_OK(mdb_env_set_maxdbs(env, 3));
    CHECK_OK(mdb_env_set_maxreaders(env, 42));
    unsigned readers = 0;
    CHECK_OK(mdb_env_get_maxreaders(env, &readers));
    CHECK(readers == 42);
    CHECK_OK(mdb_env_set_flags(env, MDB_NOSYNC, 1));
    CHECK_OK(mdb_env_set_flags(env, MDB_NOMETASYNC, 1));
    CHECK_OK(mdb_env_set_flags(env, MDB_NOMETASYNC, 0));
    unsigned flags = 0;
    CHECK_OK(mdb_env_get_flags(env, &flags));
    CHECK(flags == MDB_NOSYNC);

    const char *path = (const char *)0x1;
    CHECK_OK(mdb_env_get_path(env, &path));
    CHECK(path == NULL);
    mdb_filehandle_t fd;
    CHECK_RC(mdb_env_get_fd(env, &fd), EINVAL);

    /* userctx round-trip, never forwarded */
    int cookie;
    CHECK_OK(mdb_env_set_userctx(env, &cookie));
    CHECK(mdb_env_get_userctx(env) == &cookie);

    /* pre-open-only ops on unopened env */
    MDB_txn *txn;
    CHECK_RC(mdb_txn_begin(env, NULL, 0, &txn), EINVAL);
    MDB_stat st;
    CHECK_RC(mdb_env_stat(env, &st), EINVAL);

    /* open and verify everything took effect */
    const char *dir = at_dir("env");
    CHECK_OK(mdb_env_open(env, dir, 0, 0644));
    CHECK(mdb_env_get_userctx(env) == &cookie);
    readers = 0;
    CHECK_OK(mdb_env_get_maxreaders(env, &readers));
    CHECK(readers == 42);
    CHECK_OK(mdb_env_get_flags(env, &flags));
    CHECK((flags & MDB_NOSYNC) != 0);
    CHECK((flags & MDB_NOMETASYNC) == 0);
    MDB_envinfo info;
    CHECK_OK(mdb_env_info(env, &info));
    CHECK(info.me_mapsize == 4u << 20);
    CHECK_OK(mdb_env_get_path(env, &path));
    CHECK(path != NULL && strcmp(path, dir) == 0);
    CHECK_OK(mdb_env_get_fd(env, &fd));
    CHECK(fd >= 0);
    CHECK(mdb_env_get_maxkeysize(env) == 511); /* 0.9 engine constant */

    /* maxdbs took effect: 3 named DBs ok */
    CHECK_OK(mdb_txn_begin(env, NULL, 0, &txn));
    MDB_dbi dbi;
    CHECK_OK(mdb_dbi_open(txn, "a", MDB_CREATE, &dbi));
    CHECK_OK(mdb_dbi_open(txn, "b", MDB_CREATE, &dbi));
    CHECK_OK(mdb_dbi_open(txn, "c", MDB_CREATE, &dbi));
    CHECK_OK(mdb_txn_commit(txn));

    mdb_env_close(env);

    /* double open rejected */
    CHECK_OK(mdb_env_create(&env));
    CHECK_OK(mdb_env_open(env, dir, 0, 0644));
    CHECK_RC(mdb_env_open(env, dir, 0, 0644), EINVAL);
    mdb_env_close(env);
    return 0;
}
