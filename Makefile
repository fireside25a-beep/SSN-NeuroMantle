CC ?= cc
CXX ?= c++
CPPFLAGS := -Iinclude -Isrc -Ivendor/neuroroute/include -Ivendor/surreal/include
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
CXXFLAGS := -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror
C_OBJECTS := build/neuro_mantle.o build/neuro_mantle_ecc.o build/neuro_sandbox.o build/routing.o build/spatial_pool.o
CXX_OBJECTS := build/main.o build/ai.o build/state_tree.o build/hash.o build/exact.o build/causal.o build/entropy.o
.PHONY: all test sanitize clean devcli vanta-check companions qecc-check benchmark
all: bin/neuromantle devcli
devcli:
	$(MAKE) -C vendor/devcli devcli
vanta-check:
	$(MAKE) -C vendor/vanta test
companions: all
	python3 tests/check_companions.py
qecc-check: bin/neuromantle
	python3 tests/qecc_integration.py
build bin:
	mkdir -p $@
build/neuro_mantle.o: core/neuro_mantle.c include/neuro_mantle.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/neuro_mantle_ecc.o: core/neuro_mantle_ecc.c include/neuro_mantle_ecc.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/neuro_sandbox.o: core/neuro_sandbox.c include/neuro_sandbox.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/routing.o: vendor/neuroroute/src/routing.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/spatial_pool.o: vendor/neuroroute/src/spatial_pool.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/main.o: src/main.cpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
build/ai.o: src/ai.cpp src/ai.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
build/state_tree.o: src/state_tree.cpp src/state_tree.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
build/%.o: vendor/surreal/src/%.cpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
bin/neuromantle: $(C_OBJECTS) $(CXX_OBJECTS) | bin
	$(CXX) $(CXXFLAGS) $^ -o $@
bin/test_core: tests/test_core.c build/neuro_mantle.o | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@
bin/test_ecc: tests/test_ecc.c build/neuro_mantle_ecc.o | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@
bin/test_state_tree: tests/state_tree_test.cpp build/state_tree.o build/hash.o build/neuro_mantle.o build/routing.o build/spatial_pool.o | bin
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@
bin/bigint_probe: tests/bigint_probe.cpp vendor/surreal/include/surreal/bigint.hpp | bin
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@
bin/state_probe: tests/state_probe.c build/neuro_mantle.o | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@
bin/bench_core: bench/bench_core.c build/neuro_mantle.o | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@
benchmark: bin/bench_core
	python3 scripts/run_benchmarks.py
	python3 scripts/run_end_to_end_benchmarks.py
test: all bin/test_core bin/test_ecc bin/test_state_tree bin/bigint_probe bin/state_probe
	./bin/test_core
	./bin/test_ecc
	./bin/test_state_tree
	python3 tests/integration.py
	python3 tests/sandbox_integration.py
	python3 tests/ai_integration.py
	python3 tests/socket_integration.py
	python3 tests/bigint_differential.py
	python3 tests/network_oracle.py
sanitize:
	$(MAKE) clean
	NM_BIGINT_CASES_PER_SIZE=10 $(MAKE) CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' CXXFLAGS='-std=c++23 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' test
clean:
	rm -rf build bin
