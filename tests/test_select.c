/* Engine selection: per-env setter, process default, precedence, and the
 * rule that an existing file's format always wins. */
#include "anytest.h"

int
main(int argc, char **argv)
{
    at_init(argc, argv);
    anylmdb_ver ver;

    /* per-env setter picks 1.0 */
    const char *d10 = at_dir("v10");
    MDB_env *env = at_env_open(d10, ANYLMDB_VER_10, 0, 0);
    CHECK_OK(anylmdb_env_get_version(env, &ver));
    CHECK(ver == ANYLMDB_VER_10);
    /* setter after open fails */
    CHECK_RC(anylmdb_env_set_version(env, ANYLMDB_VER_09), EINVAL);
    mdb_env_close(env);

    /* process-global default */
    const char *dglob = at_dir("glob");
    CHECK_OK(anylmdb_set_default_version(ANYLMDB_VER_10));
    env = at_env_open(dglob, ANYLMDB_VER_DEFAULT, 0, 0);
    CHECK_OK(anylmdb_env_get_version(env, &ver));
    CHECK(ver == ANYLMDB_VER_10);
    mdb_env_close(env);

    /* per-env setter beats the global default */
    const char *dprec = at_dir("prec");
    env = at_env_open(dprec, ANYLMDB_VER_09, 0, 0);
    CHECK_OK(anylmdb_env_get_version(env, &ver));
    CHECK(ver == ANYLMDB_VER_09);
    mdb_env_close(env);
    CHECK_OK(anylmdb_set_default_version(ANYLMDB_VER_DEFAULT));

    /* an existing file's format wins over the setter */
    env = at_env_open(d10, ANYLMDB_VER_09, 0, 0); /* d10 holds a 1.0 file */
    CHECK_OK(anylmdb_env_get_version(env, &ver));
    CHECK(ver == ANYLMDB_VER_10);
    mdb_env_close(env);

    /* invalid values rejected */
    CHECK_RC(anylmdb_set_default_version((anylmdb_ver)7), EINVAL);
    CHECK_OK(mdb_env_create(&env));
    CHECK_RC(anylmdb_env_set_version(env, (anylmdb_ver)7), EINVAL);
    mdb_env_close(env);
    return 0;
}
