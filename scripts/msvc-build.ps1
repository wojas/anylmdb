# Build anylmdb with MSVC and run the test suite. Requires cl/lib on PATH
# (a Visual Studio developer prompt, or ilammy/msvc-dev-cmd in CI).
#
# This mirrors how py-lmdb-style consumers build anylmdb on Windows:
# compile the eight anylmdb_*.c files, nothing else — no Makefile involved.
# On Windows anylmdb_mdb10.c compiles mdb_lmdb10_win.c.h, the pristine 1.0
# source plus the minimal mdb_lmdb10_win.patch (upstream 1.0.0 does not
# compile for Windows unpatched; see scripts/gen-win10.sh).
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

$cflags = @(
    '/nologo', '/W3', '/O2', '/MD', '/std:c11',
    '/D_CRT_SECURE_NO_WARNINGS', '/D_CRT_NONSTDC_NO_DEPRECATE'
)
$sources = @(
    'anylmdb_mdb09.c', 'anylmdb_midl09.c', 'anylmdb_mdb10.c', 'anylmdb_midl10.c',
    'anylmdb_glue09.c', 'anylmdb_glue10.c', 'anylmdb_core.c', 'anylmdb_sniff.c'
)

cl @cflags /c @sources
if ($LASTEXITCODE) { exit 1 }
lib /nologo /OUT:anylmdb.lib ($sources -replace '\.c$', '.obj')
if ($LASTEXITCODE) { exit 1 }

New-Item -ItemType Directory -Force -Path tests\build | Out-Null
$tests = @(
    'test_defaults', 'test_select', 'test_crud', 'test_txn',
    'test_v10only', 'test_preopen', 'test_sniff', 'test_cross'
)
foreach ($t in $tests) {
    cl @cflags /I. "/Fetests\build\$t.exe" "/Fotests\build\$t.obj" `
        "tests\$t.c" anylmdb.lib advapi32.lib
    if ($LASTEXITCODE) { exit 1 }
}

$failed = 0
foreach ($t in $tests) {
    $tmp = Join-Path $env:TEMP ("anylmdb-" + $t + "-" + [guid]::NewGuid())
    New-Item -ItemType Directory -Path $tmp | Out-Null
    & "tests\build\$t.exe" $tmp
    if ($LASTEXITCODE) {
        Write-Host "FAIL $t"
        $failed = 1
    } else {
        Write-Host "PASS $t"
    }
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
if ($failed) { exit 1 }
Write-Host "msvc-build: all tests passed"
