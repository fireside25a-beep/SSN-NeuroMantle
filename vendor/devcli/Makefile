CC ?= cc
AR ?= ar

CPPFLAGS := -Iinclude
WARN_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror
CFLAGS ?= -O2
LDLIBS :=

LIB_SOURCES := src/routing.c src/spatial_pool.c
LIB_OBJECTS := $(LIB_SOURCES:src/%.c=build/%.o)
LIBRARY := lib/libneuro_router.a

DEVCLI_SOURCES := \
	src/devcli.c \
	src/fs_utils.c \
	src/main.c \
	src/process_runner.c \
	src/project_actions.c \
	src/project_detect.c \
	src/project_init.c \
	src/toolchains.c
DEVCLI_OBJECTS := $(DEVCLI_SOURCES:src/%.c=build/%.o)
DEVCLI := bin/neuro

TEST_ROUTING := bin/test_routing
TEST_SPATIAL := bin/test_spatial_pool
TEST_PROCESS := bin/test_process_runner
TEST_DETECT := bin/test_project_detect
TEST_INIT := bin/test_project_init
TEST_BINARIES := $(TEST_ROUTING) $(TEST_SPATIAL) $(TEST_PROCESS) $(TEST_DETECT) $(TEST_INIT)
DEMO := bin/neuro_demo

.PHONY: all test demo devcli debug sanitize clean

all: $(LIBRARY) $(TEST_BINARIES) $(DEMO) $(DEVCLI)

devcli: $(DEVCLI)

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(WARN_CFLAGS) $(CFLAGS) -c $< -o $@

$(LIBRARY): $(LIB_OBJECTS)
	@mkdir -p lib
	$(AR) rcs $@ $^

$(TEST_ROUTING): tests/test_routing.c $(LIBRARY)
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(WARN_CFLAGS) $(CFLAGS) $< $(LIBRARY) $(LDLIBS) -o $@

$(TEST_SPATIAL): tests/test_spatial_pool.c $(LIBRARY)
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(WARN_CFLAGS) $(CFLAGS) $< $(LIBRARY) $(LDLIBS) -o $@

$(TEST_PROCESS): tests/test_process_runner.c build/process_runner.o
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(WARN_CFLAGS) $(CFLAGS) $^ $(LDLIBS) -o $@

$(TEST_DETECT): tests/test_project_detect.c build/project_detect.o build/fs_utils.o
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(WARN_CFLAGS) $(CFLAGS) $^ $(LDLIBS) -o $@

$(TEST_INIT): tests/test_project_init.c build/project_init.o build/project_detect.o build/fs_utils.o
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(WARN_CFLAGS) $(CFLAGS) $^ $(LDLIBS) -o $@

$(DEMO): examples/demo.c $(LIBRARY)
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(WARN_CFLAGS) $(CFLAGS) $< $(LIBRARY) $(LDLIBS) -o $@

$(DEVCLI): $(DEVCLI_OBJECTS)
	@mkdir -p bin
	$(CC) $(WARN_CFLAGS) $(CFLAGS) $^ $(LDLIBS) -o $@

test: $(TEST_BINARIES) $(DEVCLI)
	./$(TEST_ROUTING)
	./$(TEST_SPATIAL)
	./$(TEST_PROCESS)
	./$(TEST_DETECT)
	./$(TEST_INIT)
	./tests/test_cli.sh ./$(DEVCLI)

demo: $(DEMO)
	./$(DEMO)

debug: CFLAGS := -O0 -g3
debug: clean all test demo

sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS="-O1 -g3 -fno-omit-frame-pointer -fsanitize=address,undefined" all
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./$(TEST_ROUTING)
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./$(TEST_SPATIAL)
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./$(TEST_PROCESS)
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./$(TEST_DETECT)
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./$(TEST_INIT)
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./$(DEMO)
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./tests/test_cli.sh ./$(DEVCLI)

clean:
	rm -rf build bin lib
