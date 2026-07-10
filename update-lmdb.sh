#!/bin/sh
# Update one vendored LMDB stream from upstream.
#
#   ./update-lmdb.sh 09 0.9.35
#   ./update-lmdb.sh 10 1.0.0
#
# Downloads the release tag from the GitHub mirror, copies the pristine
# sources into place (byte-identical, no modifications of any kind),
# regenerates the symbol-rename header, and records the update in VERSIONS.
#
# IMPORTANT: download ONLY from the GitHub mirror. Do NOT switch this to
# git.openldap.org — repeated tarball downloads from there have gotten an
# IP blocked before. The mirror repo contains only libraries/liblmdb, so
# the tarballs are small.

set -eu

if [ $# -ne 2 ]; then
    echo "usage: $0 <stream> <version>   e.g.: $0 09 0.9.35 | $0 10 1.0.0" >&2
    exit 1
fi

stream=$1
version=$2

case "$stream" in
09|10) ;;
*) echo "error: stream must be 09 or 10" >&2; exit 1 ;;
esac

cd "$(dirname "$0")"

url="https://github.com/LMDB/lmdb/archive/refs/tags/LMDB_${version}.tar.gz"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

echo "Downloading $url"
curl -fsSL -o "$tmp/lmdb.tar.gz" "$url"

if command -v sha256sum >/dev/null 2>&1; then
    sha256=$(sha256sum "$tmp/lmdb.tar.gz" | awk '{print $1}')
else
    sha256=$(shasum -a 256 "$tmp/lmdb.tar.gz" | awk '{print $1}')
fi

tar -C "$tmp" -xzf "$tmp/lmdb.tar.gz"
src="$tmp/lmdb-LMDB_${version}/libraries/liblmdb"
[ -d "$src" ] || { echo "error: $src not found in tarball" >&2; exit 1; }

# Pristine copies. The engine sources get a .c.h suffix so that cgo does not
# compile them directly (the wrapper anylmdb_*.c TUs include them after the
# rename header) while `go mod vendor` still retains them.
cp "$src/mdb.c"      "mdb_lmdb${stream}.c.h"
cp "$src/midl.c"     "midl_lmdb${stream}.c.h"
cp "$src/lmdb.h"     "lmdb_lmdb${stream}.h"
cp "$src/midl.h"     "midl_lmdb${stream}.h"
cp "$src/CHANGES"    "CHANGES.lmdb${stream}.txt"
cp "$src/LICENSE"    "LICENSE.lmdb${stream}"
cp "$src/COPYRIGHT"  "COPYRIGHT.lmdb${stream}"

# Verify the copies really are byte-identical to upstream.
for pair in "mdb.c mdb_lmdb${stream}.c.h" "midl.c midl_lmdb${stream}.c.h" \
            "lmdb.h lmdb_lmdb${stream}.h" "midl.h midl_lmdb${stream}.h"; do
    set -- $pair
    cmp "$src/$1" "$2" || { echo "error: $2 is not pristine" >&2; exit 1; }
done

echo "Regenerating anylmdb_rename${stream}.h"
scripts/gen-rename.sh "$stream"

if [ "$stream" = 10 ]; then
    echo "Regenerating mdb_lmdb10_win.c.h (Windows compile fixes)"
    scripts/gen-win10.sh
fi

# Record the update, replacing any previous entry for this stream.
date=$(date -u +%Y-%m-%d)
tmpv="$tmp/VERSIONS"
[ -f VERSIONS ] && grep -v "^$stream " VERSIONS > "$tmpv" || true
echo "$stream $version $date sha256:$sha256 $url" >> "$tmpv"
sort "$tmpv" > VERSIONS

echo "Done. Now run: make clean all check-symbols test"
