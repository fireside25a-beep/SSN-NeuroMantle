SHELL := /bin/bash
CXX ?= g++
FC = gfortran
ASM := gcc
BUILD ?= build
CPPFLAGS := -Iinclude $(EXTRA_CPPFLAGS)
CXXFLAGS_BASE := -MMD -MP -std=c++23 -O2 -Wall -Wextra -Werror -Wpedantic -Wshadow -fstack-protector-strong -D_FORTIFY_SOURCE=3 -ffunction-sections -fdata-sections -fPIE
FFLAGS_BASE := -std=f2018 -O2 -Wall -Wextra -Werror -fstack-protector-strong -fPIE
ASFLAGS_BASE := -fPIE
LDFLAGS_BASE := -Wl,-z,relro,-z,now,--gc-sections -pie
LDLIBS := -lgfortran

CPP_SRC := src/admission.cpp src/io.cpp src/hash.cpp src/exact.cpp src/entropy.cpp src/units.cpp src/field_kernels.cpp src/causal.cpp src/physics.cpp src/universe.cpp src/rewrite.cpp src/horizon.cpp src/qecc.cpp
CPP_OBJ := $(patsubst src/%.cpp,$(BUILD)/%.o,$(CPP_SRC))
F_OBJ := $(BUILD)/field_kernels_f.o
A_OBJ := $(BUILD)/entropy_asm.o
CORE_OBJ := $(CPP_OBJ) $(F_OBJ) $(A_OBJ)
TESTS := test_core test_runtime test_failures test_admission test_qecc test_proof_properties

.PHONY: all test qec-dual-test test-asan test-ubsan test-clang audit clean
all: $(BUILD)/surreal

LLRC := toolchains/llrlang-0.5.0/bin/llrc-linux-x86_64

$(BUILD)/qecc_llr_verify: llrlang/qecc_verify.llr $(LLRC) | $(BUILD)
	$(LLRC) $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS_BASE) $(CXXFLAGS) -c $< -o $@

$(F_OBJ): fortran/field_kernels.f90 | $(BUILD)
	$(FC) $(FFLAGS_BASE) $(FFLAGS) -J$(BUILD) -c $< -o $@

$(A_OBJ): src/entropy.S | $(BUILD)
	$(ASM) $(ASFLAGS_BASE) $(ASFLAGS) -c $< -o $@

$(BUILD)/main.o: src/main.cpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS_BASE) $(CXXFLAGS) -c $< -o $@

$(BUILD)/surreal: $(CORE_OBJ) $(BUILD)/main.o
	$(CXX) $(LDFLAGS_BASE) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/test_%: tests/test_%.cpp $(CORE_OBJ) tests/test.hpp $(wildcard include/surreal/*.hpp)
	$(CXX) $(CPPFLAGS) -Itests $(CXXFLAGS_BASE) $(CXXFLAGS) $< $(CORE_OBJ) $(LDFLAGS_BASE) $(LDFLAGS) $(LDLIBS) -o $@

test: $(addprefix $(BUILD)/,$(TESTS)) $(BUILD)/surreal $(BUILD)/qecc_llr_verify
	@set -e; for t in $(TESTS); do echo "== $$t =="; ./$(BUILD)/$$t; done
	@rm -rf $(BUILD)/smoke-run
	./$(BUILD)/surreal demo 2 $(BUILD)/smoke-run 0x1234 lcdm >/dev/null
	./$(BUILD)/surreal verify $(BUILD)/smoke-run
	./$(BUILD)/surreal laws >/dev/null
	./$(BUILD)/surreal formulas >/dev/null
	./$(BUILD)/surreal math >/dev/null
	@printf 'SURREAL_ADMISSION_REQUEST_V1\nrequest_id=smoke\noperation=exact.compare\nlhs=2/4\nrhs=1/2\nrelation=eq\n' > $(BUILD)/admission.req
	./$(BUILD)/surreal admit $(BUILD)/admission.req > $(BUILD)/admission.receipt
	./$(BUILD)/surreal verify-receipt $(BUILD)/admission.receipt
	./$(BUILD)/surreal describe >/dev/null
	@rm -rf $(BUILD)/qecc && mkdir -p $(BUILD)/qecc
	./$(BUILD)/surreal qec-prove $(BUILD)/qecc > $(BUILD)/qecc/proof.stdout
	BUILD=$(BUILD) bash scripts/verify_qecc_dual.sh $(BUILD)/qecc/five_qubit.sqecc

qec-dual-test: $(BUILD)/surreal $(BUILD)/qecc_llr_verify
	@rm -rf $(BUILD)/qecc && mkdir -p $(BUILD)/qecc
	./$(BUILD)/surreal qec-prove $(BUILD)/qecc > $(BUILD)/qecc/proof.stdout
	BUILD=$(BUILD) bash scripts/verify_qecc_dual.sh $(BUILD)/qecc/five_qubit.sqecc

test-asan:
	$(MAKE) test BUILD=build-asan CXXFLAGS='-O0 -g1 -fsanitize=address -fno-omit-frame-pointer' FFLAGS='-O0 -g1 -fsanitize=address -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address'

test-ubsan:
	$(MAKE) test BUILD=build-ubsan CXXFLAGS='-O0 -g1 -fsanitize=undefined -fno-omit-frame-pointer' FFLAGS='-O0 -g1 -fsanitize=undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=undefined'

test-clang:
	$(MAKE) test BUILD=build-clang CXX=clang++ ASM=clang

audit: $(BUILD)/surreal $(BUILD)/qecc_llr_verify $(addprefix $(BUILD)/,$(TESTS))
	BUILD=$(BUILD) bash scripts/audit.sh

clean:
	rm -rf build build-release build-asan build-ubsan build-clang build-final

-include $(wildcard $(BUILD)/*.d)

.PHONY: industry-test proof-mutation-test
industry-test: $(BUILD)/surreal $(BUILD)/qecc_llr_verify
	python3 tests/industry_suite.py --build $(BUILD)

proof-mutation-test: $(BUILD)/test_qecc
	python3 tests/industry_mutation.py --build $(BUILD)
