/* Minimal test harness for the anylmdb suite. Each test binary takes a
 * fresh scratch directory as argv[1] and exits non-zero on failure. */
#ifndef ANYTEST_H
#define ANYTEST_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "anylmdb.h"

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while (0)

#define CHECK_RC(expr, want) do { \
    int rc_ = (expr); \
    if (rc_ != (want)) { \
        fprintf(stderr, "%s:%d: %s = %d (%s), want %d (%s)\n", __FILE__, \
            __LINE__, #expr, rc_, mdb_strerror(rc_), (int)(want), \
            mdb_strerror(want)); \
        exit(1); \
    } \
} while (0)

#define CHECK_OK(expr) CHECK_RC(expr, MDB_SUCCESS)

static const char *at_scratch;

/* Returns a fresh subdirectory of the scratch dir. Rotates through static
 * buffers so several results can be held at once. */
static inline const char *
at_dir(const char *name)
{
    static char bufs[16][1024];
    static int i;
    char *buf = bufs[i++ % 16];
    snprintf(buf, 1024, "%s/%s", at_scratch, name);
    CHECK(mkdir(buf, 0755) == 0);
    return buf;
}

static inline void
at_init(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <scratch-dir>\n", argv[0]);
        exit(2);
    }
    at_scratch = argv[1];
}

/* Create and open an env in one call; ANYLMDB_VER_DEFAULT leaves the
 * format choice to anylmdb's defaults. */
static inline MDB_env *
at_env_open(const char *dir, anylmdb_ver ver, unsigned flags, size_t mapsize)
{
    MDB_env *env;
    CHECK_OK(mdb_env_create(&env));
    if (ver != ANYLMDB_VER_DEFAULT)
        CHECK_OK(anylmdb_env_set_version(env, ver));
    if (mapsize)
        CHECK_OK(mdb_env_set_mapsize(env, mapsize));
    CHECK_OK(mdb_env_open(env, dir, flags, 0644));
    return env;
}

#endif /* ANYTEST_H */
