# anylmdb — one library carrying both LMDB 0.9 and LMDB 1.0.
# POSIX only (Linux + macOS).

CC      ?= cc
CFLAGS  ?= -O2 -g
WARN     = -Wall -Wextra
BASEFLAGS = $(WARN) -pthread -fPIC $(XCFLAGS)
PREFIX  ?= /usr/local

UNAME_S := $(shell uname -s)

OBJS = anylmdb_mdb09.o anylmdb_midl09.o anylmdb_mdb10.o anylmdb_midl10.o \
       anylmdb_glue09.o anylmdb_glue10.o anylmdb_core.o anylmdb_sniff.o

# Files a consumer copies to vendor anylmdb (flat, no build-time codegen).
VENDOR_FILES = lmdb.h midl.h anylmdb.h anylmdb_ops.h anylmdb_int.h \
       anylmdb_rename09.h anylmdb_rename10.h \
       lmdb_lmdb09.h midl_lmdb09.h lmdb_lmdb10.h midl_lmdb10.h \
       mdb_lmdb09.c.h midl_lmdb09.c.h mdb_lmdb10.c.h midl_lmdb10.c.h \
       anylmdb_mdb09.c anylmdb_midl09.c anylmdb_mdb10.c anylmdb_midl10.c \
       anylmdb_glue09.c anylmdb_glue10.c anylmdb_core.c anylmdb_sniff.c \
       LICENSE LICENSE.lmdb09 COPYRIGHT.lmdb09 LICENSE.lmdb10 COPYRIGHT.lmdb10

ifeq ($(UNAME_S),Darwin)
SHARED_LIB   = libanylmdb.dylib
SHARED_FLAGS = -dynamiclib -install_name @rpath/libanylmdb.dylib \
               -Wl,-exported_symbols_list,anylmdb.exp
DROPIN_LIBS  = liblmdb.0.dylib liblmdb.1.dylib
else
SHARED_LIB   = libanylmdb.so
SHARED_FLAGS = -shared -Wl,-soname,libanylmdb.so.0 \
               -Wl,--version-script=anylmdb.sym
DROPIN_LIBS  = liblmdb.so.0 liblmdb.so.1
endif

all: libanylmdb.a $(SHARED_LIB)

# Engine objects compile pristine upstream code: keep -Wall, skip -Wextra.
anylmdb_mdb09.o anylmdb_midl09.o anylmdb_mdb10.o anylmdb_midl10.o: WARN = -Wall

%.o: %.c
	$(CC) $(CFLAGS) $(BASEFLAGS) -c -o $@ $<

libanylmdb.a: $(OBJS)
	ar rcs $@ $(OBJS)

$(SHARED_LIB): $(OBJS) anylmdb.sym anylmdb.exp
	$(CC) $(CFLAGS) $(BASEFLAGS) $(SHARED_FLAGS) -o $@ $(OBJS)

# Drop-in builds carrying liblmdb's usual sonames, for repointing or
# LD_PRELOADing existing binaries without a relink. Not installed by default.
dropin: $(DROPIN_LIBS)

ifeq ($(UNAME_S),Darwin)
liblmdb.0.dylib: $(OBJS) anylmdb.exp
	$(CC) $(CFLAGS) $(BASEFLAGS) -dynamiclib -install_name @rpath/liblmdb.0.dylib \
		-Wl,-exported_symbols_list,anylmdb.exp -o $@ $(OBJS)
liblmdb.1.dylib: $(OBJS) anylmdb.exp
	$(CC) $(CFLAGS) $(BASEFLAGS) -dynamiclib -install_name @rpath/liblmdb.1.dylib \
		-Wl,-exported_symbols_list,anylmdb.exp -o $@ $(OBJS)
else
liblmdb.so.0: $(OBJS) anylmdb.sym
	$(CC) $(CFLAGS) $(BASEFLAGS) -shared -Wl,-soname,liblmdb.so.0 \
		-Wl,--version-script=anylmdb.sym -o $@ $(OBJS)
liblmdb.so.1: $(OBJS) anylmdb.sym
	$(CC) $(CFLAGS) $(BASEFLAGS) -shared -Wl,-soname,liblmdb.so.1 \
		-Wl,--version-script=anylmdb.sym -o $@ $(OBJS)
endif

# Fail if any engine object leaks an unprefixed mdb_* symbol (rename-header
# regression), or if the wrapper objects miss/leak public symbols.
check-symbols: $(OBJS)
	@fail=0; \
	for o in anylmdb_mdb09.o anylmdb_midl09.o anylmdb_mdb10.o anylmdb_midl10.o \
	         anylmdb_glue09.o anylmdb_glue10.o; do \
	    bad=$$(nm -gP $$o | awk '$$2 != "U" && $$2 != "" {print $$1}' \
	           | sed 's/^_//' | grep -E '^mdb_[a-z]' || true); \
	    if [ -n "$$bad" ]; then \
	        echo "FAIL: unprefixed extern symbols in $$o:"; echo "$$bad"; fail=1; \
	    fi; \
	done; \
	missing=$$(nm -gP anylmdb_core.o anylmdb_sniff.o | awk '$$2 != "U" && $$2 != "" {print $$1}' \
	           | sed 's/^_//' | grep -E '^(mdb|anylmdb)_' | sort > .exports.actual; \
	           diff expected-symbols.txt .exports.actual || true); \
	rm -f .exports.actual; \
	if [ -n "$$missing" ]; then \
	    echo "FAIL: public symbol list drifted from expected-symbols.txt:"; \
	    echo "$$missing"; fail=1; \
	fi; \
	[ $$fail -eq 0 ] && echo "check-symbols: OK" || exit 1

# Regenerate the expected public symbol list (review the diff before committing).
expected-symbols: anylmdb_core.o anylmdb_sniff.o
	nm -gP anylmdb_core.o anylmdb_sniff.o | awk '$$2 != "U" && $$2 != "" {print $$1}' \
	    | sed 's/^_//' | grep -E '^(mdb|anylmdb)_' | sort > expected-symbols.txt

TESTS = test_defaults test_select test_crud test_txn test_v10only \
        test_preopen test_sniff test_cross
TESTBIN = $(TESTS:%=tests/build/%)

tests/build/%: tests/%.c libanylmdb.a tests/anytest.h
	@mkdir -p tests/build
	$(CC) $(CFLAGS) $(WARN) -pthread $(TESTCFLAGS) -I. -o $@ $< libanylmdb.a

test: all check-symbols $(TESTBIN)
	@rc=0; \
	for t in $(TESTS); do \
	    tmp=$$(mktemp -d); \
	    if ./tests/build/$$t "$$tmp" >/dev/null 2>tests/build/$$t.err; then \
	        echo "PASS $$t"; \
	    else \
	        echo "FAIL $$t"; cat tests/build/$$t.err; rc=1; \
	    fi; \
	    rm -rf "$$tmp"; \
	done; exit $$rc

# Same suite under AddressSanitizer.
test-asan:
	$(MAKE) clean
	$(MAKE) test CFLAGS="-O1 -g -fsanitize=address" TESTCFLAGS="-fsanitize=address"
	$(MAKE) clean

# Drop-in check: a plain-lmdb client linked against the liblmdb.so.1-soname
# build must load anylmdb and work. Linux-only (soname/rpath semantics).
ifeq ($(UNAME_S),Darwin)
test-dropin:
	@echo "test-dropin: skipped on macOS (Linux-only drop-in check)"
else
test-dropin: liblmdb.so.1
	@mkdir -p tests/build
	$(CC) $(CFLAGS) $(WARN) -pthread -I. -o tests/build/test_dropin \
		tests/test_dropin.c ./liblmdb.so.1
	@tmp=$$(mktemp -d); \
	if LD_LIBRARY_PATH=. ./tests/build/test_dropin "$$tmp"; then \
	    echo "PASS test_dropin"; rc=0; \
	else \
	    echo "FAIL test_dropin"; rc=1; \
	fi; \
	rm -rf "$$tmp"; exit $$rc
endif

# Run the full suite in Linux containers (Debian glibc + Alpine musl).
docker-test:
	docker build -f docker/debian.Dockerfile -t anylmdb-test-debian .
	docker build -f docker/alpine.Dockerfile -t anylmdb-test-alpine .
	@echo "docker-test: OK (debian + alpine)"

vendor-list:
	@printf '%s\n' $(VENDOR_FILES)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/lib $(DESTDIR)$(PREFIX)/include
	cp lmdb.h lmdb_lmdb10.h anylmdb.h $(DESTDIR)$(PREFIX)/include/
	cp libanylmdb.a $(SHARED_LIB) $(DESTDIR)$(PREFIX)/lib/

clean:
	rm -rf *.o *.a *.so *.so.* *.dylib tests/build .exports.actual

.PHONY: all dropin check-symbols expected-symbols test test-asan test-dropin \
        docker-test vendor-list install clean
