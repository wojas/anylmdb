/* Sniffer: real files in both layouts, NOSUBDIR, hand-crafted unsupported
 * and garbage files, empty/new cases. */
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "anytest.h"

static void
write_file(const char *path, const unsigned char *buf, size_t len)
{
#ifdef _WIN32
    int fd = _open(path, _O_CREAT | _O_WRONLY | _O_TRUNC | _O_BINARY, 0644);
    CHECK(fd >= 0);
    CHECK(_write(fd, buf, (unsigned)len) == (int)len);
    _close(fd);
#else
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    CHECK(fd >= 0);
    CHECK(write(fd, buf, len) == (ssize_t)len);
    close(fd);
#endif
}

static void
put32(unsigned char *p, unsigned v)
{
    memcpy(p, &v, 4);
}

int
main(int argc, char **argv)
{
    at_init(argc, argv);
    anylmdb_ver ver;
    char path[1024];
    unsigned char buf[4096];

    /* real environments, subdir layout */
    const char *d09 = at_dir("e09");
    mdb_env_close(at_env_open(d09, ANYLMDB_VER_09, 0, 0));
    CHECK_OK(anylmdb_sniff(d09, 0, &ver));
    CHECK(ver == ANYLMDB_VER_09);

    const char *d10 = at_dir("e10");
    mdb_env_close(at_env_open(d10, ANYLMDB_VER_10, 0, 0));
    CHECK_OK(anylmdb_sniff(d10, 0, &ver));
    CHECK(ver == ANYLMDB_VER_10);

    /* real environments, NOSUBDIR layout */
    snprintf(path, sizeof path, "%s/f09.mdb", at_scratch);
    mdb_env_close(at_env_open(path, ANYLMDB_VER_09, MDB_NOSUBDIR, 0));
    CHECK_OK(anylmdb_sniff(path, MDB_NOSUBDIR, &ver));
    CHECK(ver == ANYLMDB_VER_09);

    snprintf(path, sizeof path, "%s/f10.mdb", at_scratch);
    mdb_env_close(at_env_open(path, ANYLMDB_VER_10, MDB_NOSUBDIR, 0));
    CHECK_OK(anylmdb_sniff(path, MDB_NOSUBDIR, &ver));
    CHECK(ver == ANYLMDB_VER_10);

    /* and opening the NOSUBDIR file with no hints sniffs right */
    MDB_env *env = at_env_open(path, ANYLMDB_VER_DEFAULT, MDB_NOSUBDIR, 0);
    CHECK_OK(anylmdb_env_get_version(env, &ver));
    CHECK(ver == ANYLMDB_VER_10);
    mdb_env_close(env);

    /* nonexistent → NEW */
    snprintf(path, sizeof path, "%s/nope", at_scratch);
    CHECK_OK(anylmdb_sniff(path, MDB_NOSUBDIR, &ver));
    CHECK(ver == ANYLMDB_VER_NEW);
    /* existing dir without data file → NEW */
    CHECK_OK(anylmdb_sniff(at_dir("emptydir"), 0, &ver));
    CHECK(ver == ANYLMDB_VER_NEW);

    /* empty file → NEW */
    snprintf(path, sizeof path, "%s/empty.mdb", at_scratch);
    write_file(path, buf, 0);
    CHECK_OK(anylmdb_sniff(path, MDB_NOSUBDIR, &ver));
    CHECK(ver == ANYLMDB_VER_NEW);

    /* mm_version 2 in the 0.9 layout */
    memset(buf, 0, sizeof buf);
    put32(buf + 16, 0xBEEFC0DE);
    put32(buf + 20, 2);
    snprintf(path, sizeof path, "%s/js16.mdb", at_scratch);
    write_file(path, buf, sizeof buf);
    CHECK_RC(anylmdb_sniff(path, MDB_NOSUBDIR, &ver), ANYLMDB_FORMAT_UNSUPPORTED);

    /* mm_version 2 in the 1.0 layout (lmdb-js prerelease wrote this shape) */
    memset(buf, 0, sizeof buf);
    put32(buf + 24, 0xBEEFC0DE);
    put32(buf + 28, 2);
    snprintf(path, sizeof path, "%s/js24.mdb", at_scratch);
    write_file(path, buf, sizeof buf);
    CHECK_RC(anylmdb_sniff(path, MDB_NOSUBDIR, &ver), ANYLMDB_FORMAT_UNSUPPORTED);
    /* ... and mdb_env_open must fail the same way, not touch the file */
    CHECK_OK(mdb_env_create(&env));
    CHECK_RC(mdb_env_open(env, path, MDB_NOSUBDIR, 0644), ANYLMDB_FORMAT_UNSUPPORTED);
    mdb_env_close(env);

    /* unknown future version → MDB_VERSION_MISMATCH */
    memset(buf, 0, sizeof buf);
    put32(buf + 16, 0xBEEFC0DE);
    put32(buf + 20, 999); /* MDB_DEVEL data version */
    snprintf(path, sizeof path, "%s/devel.mdb", at_scratch);
    write_file(path, buf, sizeof buf);
    CHECK_RC(anylmdb_sniff(path, MDB_NOSUBDIR, &ver), MDB_VERSION_MISMATCH);

    /* garbage → MDB_INVALID */
    memset(buf, 0xAB, sizeof buf);
    snprintf(path, sizeof path, "%s/garbage.mdb", at_scratch);
    write_file(path, buf, sizeof buf);
    CHECK_RC(anylmdb_sniff(path, MDB_NOSUBDIR, &ver), MDB_INVALID);

    /* short file → MDB_INVALID */
    snprintf(path, sizeof path, "%s/short.mdb", at_scratch);
    write_file(path, buf, 8);
    CHECK_RC(anylmdb_sniff(path, MDB_NOSUBDIR, &ver), MDB_INVALID);

    /* strerror covers the anylmdb code */
    CHECK(strstr(mdb_strerror(ANYLMDB_FORMAT_UNSUPPORTED), "anylmdb") != NULL);
    return 0;
}
