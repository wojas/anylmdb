/* A brand-new environment defaults to the 0.9 format, and reopening it
 * with no hints sniffs the same format back. */
#include "anytest.h"

int
main(int argc, char **argv)
{
    at_init(argc, argv);
    const char *dir = at_dir("env");
    anylmdb_ver ver;

    MDB_env *env = at_env_open(dir, ANYLMDB_VER_DEFAULT, 0, 0);
    CHECK_OK(anylmdb_env_get_version(env, &ver));
    CHECK(ver == ANYLMDB_VER_09);
    mdb_env_close(env);

    CHECK_OK(anylmdb_sniff(dir, 0, &ver));
    CHECK(ver == ANYLMDB_VER_09);

    /* reopen without hints */
    env = at_env_open(dir, ANYLMDB_VER_DEFAULT, 0, 0);
    CHECK_OK(anylmdb_env_get_version(env, &ver));
    CHECK(ver == ANYLMDB_VER_09);
    mdb_env_close(env);

    /* mdb_version reports the wrapper identity */
    int major = -1;
    char *s = mdb_version(&major, NULL, NULL);
    CHECK(major == 1);
    CHECK(strstr(s, "ANYLMDB") != NULL);
    CHECK(strstr(s, "LMDB 0.9") != NULL && strstr(s, "LMDB 1.0") != NULL);
    return 0;
}
