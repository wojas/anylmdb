/* Format detection and the anylmdb_* extension API.
 *
 * The sniffer reads the data file's first meta page directly and never
 * opens an environment: letting the wrong engine attempt the open is not
 * safe (an LMDB open stamps the lock file with its own lock-format version
 * before it ever reads the data file, so a failed probe can break or race
 * concurrent opens by the right engine).
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "anylmdb_int.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define ANYLMDB_DATANAME "\\data.mdb" /* upstream DATANAME on Windows */
#else
#include <fcntl.h>
#include <unistd.h>
#define ANYLMDB_DATANAME "/data.mdb"
#endif

/* Meta page layout facts (see mdb.c of both streams):
 * both formats start the MDB_meta (mm_magic, mm_version, ...) right after
 * the page header, whose size is 16 bytes in 0.9 and 24 in 1.0 (1.0 added
 * an 8-byte mh_txnid). mm_magic is 0xBEEFC0DE in both; mm_version
 * (MDB_DATA_VERSION) is 1 in 0.9 and 3 in 1.0. Version 2 was written by
 * lmdb-js prerelease snapshots and is compatible with neither. */
#define ANYLMDB_MAGIC 0xBEEFC0DEu

/*
 * Process-wide default version. MSVC's C mode has no <stdatomic.h>;
 * Interlocked ops provide the equivalent there.
 */
#ifdef _MSC_VER
static volatile LONG anylmdb_default_ver;
#define ANYLMDB_DEFAULT_STORE(v) InterlockedExchange(&anylmdb_default_ver, (LONG)(v))
#define ANYLMDB_DEFAULT_LOAD() InterlockedCompareExchange(&anylmdb_default_ver, 0, 0)
#else
#include <stdatomic.h>
static atomic_int anylmdb_default_ver;
#define ANYLMDB_DEFAULT_STORE(v) atomic_store(&anylmdb_default_ver, (int)(v))
#define ANYLMDB_DEFAULT_LOAD() atomic_load(&anylmdb_default_ver)
#endif

int
anylmdb_set_default_version(anylmdb_ver ver)
{
    switch (ver) {
    case ANYLMDB_VER_DEFAULT:
    case ANYLMDB_VER_09:
    case ANYLMDB_VER_10:
        ANYLMDB_DEFAULT_STORE(ver);
        return MDB_SUCCESS;
    default:
        return EINVAL;
    }
}

int
anylmdb__default_version(void)
{
    int v = (int)ANYLMDB_DEFAULT_LOAD();
    if (v == ANYLMDB_VER_09 || v == ANYLMDB_VER_10)
        return v;
    const char *e = getenv("ANYLMDB_DEFAULT");
    if (e) {
        if (strcmp(e, "10") == 0)
            return ANYLMDB_VER_10;
        if (strcmp(e, "09") == 0 || strcmp(e, "9") == 0)
            return ANYLMDB_VER_09;
    }
    return ANYLMDB_VER_09;
}

int
anylmdb_env_set_version(MDB_env *env, anylmdb_ver ver)
{
    if (!env || env->opened)
        return EINVAL;
    switch (ver) {
    case ANYLMDB_VER_DEFAULT:
    case ANYLMDB_VER_09:
    case ANYLMDB_VER_10:
        env->req_ver = (int)ver;
        return MDB_SUCCESS;
    default:
        return EINVAL;
    }
}

int
anylmdb_env_get_version(MDB_env *env, anylmdb_ver *ver)
{
    if (!env || !ver)
        return EINVAL;
    *ver = env->opened ? (anylmdb_ver)env->ver : (anylmdb_ver)env->req_ver;
    return MDB_SUCCESS;
}

/*
 * Read the head of the data file. Returns 0 with either *absent set (file
 * does not exist) or *nread set; otherwise an error code in the platform's
 * LMDB convention (errno on POSIX, GetLastError() on Windows — the same
 * codes the engines return from failed opens, so mdb_strerror covers them).
 */
#ifdef _WIN32

static int
anylmdb__read_head(const char *path, unsigned char *buf, unsigned len,
    long *nread, int *absent)
{
    /* UTF-8 path, converted like the engines do (mdb.c utf8_to_utf16). */
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (wlen <= 0)
        return EINVAL;
    WCHAR *wpath = malloc(sizeof(WCHAR) * (size_t)wlen);
    if (!wpath)
        return ENOMEM;
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wlen);

    HANDLE h = CreateFileW(wpath, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    free(wpath);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            *absent = 1;
            return MDB_SUCCESS;
        }
        return (int)err;
    }
    DWORD got = 0;
    BOOL ok = ReadFile(h, buf, len, &got, NULL);
    DWORD err = ok ? 0 : GetLastError();
    CloseHandle(h);
    if (!ok)
        return (int)err;
    *nread = (long)got;
    return MDB_SUCCESS;
}

#else /* POSIX */

static int
anylmdb__read_head(const char *path, unsigned char *buf, unsigned len,
    long *nread, int *absent)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        if (errno == ENOENT) {
            *absent = 1;
            return MDB_SUCCESS;
        }
        return errno;
    }
    ssize_t n = pread(fd, buf, len, 0);
    int serrno = errno;
    close(fd);
    if (n < 0)
        return serrno;
    *nread = (long)n;
    return MDB_SUCCESS;
}

#endif /* _WIN32 */

static uint32_t
get32(const unsigned char *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof v);
    return v;
}

int
anylmdb_sniff(const char *path, unsigned int flags, anylmdb_ver *ver)
{
    if (!path || !ver)
        return EINVAL;

    unsigned char buf[32];
    long n = 0;
    int absent = 0;
    int rc;

    if (flags & MDB_NOSUBDIR) {
        rc = anylmdb__read_head(path, buf, sizeof buf, &n, &absent);
    } else {
        size_t plen = strlen(path) + sizeof ANYLMDB_DATANAME;
        char *datafile = malloc(plen);
        if (!datafile)
            return ENOMEM;
        snprintf(datafile, plen, "%s" ANYLMDB_DATANAME, path);
        rc = anylmdb__read_head(datafile, buf, sizeof buf, &n, &absent);
        free(datafile);
    }
    if (rc)
        return rc;
    if (absent || n == 0) {
        *ver = ANYLMDB_VER_NEW;
        return MDB_SUCCESS;
    }
    if ((size_t)n < sizeof buf)
        return MDB_INVALID;

    unsigned mmver;
    if (get32(buf + 16) == ANYLMDB_MAGIC)       /* 0.9 page header size */
        mmver = get32(buf + 20);
    else if (get32(buf + 24) == ANYLMDB_MAGIC)  /* 1.0 page header size */
        mmver = get32(buf + 28);
    else
        return MDB_INVALID;

    switch (mmver) {
    case 1:
        *ver = ANYLMDB_VER_09;
        return MDB_SUCCESS;
    case 3:
        *ver = ANYLMDB_VER_10;
        return MDB_SUCCESS;
    case 2:
        return ANYLMDB_FORMAT_UNSUPPORTED;
    default:
        /* valid magic, unknown data version (e.g. MDB_DEVEL builds) */
        return MDB_VERSION_MISMATCH;
    }
}
