#!/bin/sh
# Regenerate mdb_lmdb10_win.c.h: the pristine stream-10 source with the
# minimal Windows compile fixes from mdb_lmdb10_win.patch applied.
#
# Upstream LMDB 1.0.0 does not compile for Windows (LARGE_INTEGER union
# misuse in mdb_env_copyfd0/mdb_env_incr_loadfd; unfixed on mdb.RE/1.0 as
# of 2026-07-10). anylmdb keeps mdb_lmdb10.c.h byte-identical to upstream;
# Windows builds compile this generated variant instead (see the #ifdef in
# anylmdb_mdb10.c). Run by update-lmdb.sh for stream 10; the output is
# checked in so consumers never need this script.
set -eu

cd "$(dirname "$0")/.."

if patch -s -o mdb_lmdb10_win.c.h.tmp mdb_lmdb10.c.h < mdb_lmdb10_win.patch; then
    mv mdb_lmdb10_win.c.h.tmp mdb_lmdb10_win.c.h
    echo "mdb_lmdb10_win.c.h regenerated from patch"
else
    rm -f mdb_lmdb10_win.c.h.tmp
    echo "WARNING: mdb_lmdb10_win.patch no longer applies to mdb_lmdb10.c.h." >&2
    echo "Upstream may have fixed the Windows build. Verify, then either" >&2
    echo "rebase the patch or delete it and make mdb_lmdb10_win.c.h a plain" >&2
    echo "copy. Falling back to a pristine copy for now." >&2
    cp mdb_lmdb10.c.h mdb_lmdb10_win.c.h
    exit 1
fi
