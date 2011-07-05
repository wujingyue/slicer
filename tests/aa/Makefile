LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj
LIBRARYNAME = aa-test
LOADABLE_MODULE = 1
SOURCES = aa-test.cpp
CXXFLAGS += -I$(LLVM_ROOT)
include $(LEVEL)/Makefile.common

PROG_NAMES = aget-nocrit.simple test-overwrite.slice FFT-nocrit.slice
PROGS_DIR = ../progs

run:: $(PROG_NAMES)

%: $(PROGS_DIR)/%.bc
	opt -disable-output \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libLLVMDataStructure.so \
		-load $(LLVM_ROOT)/install/lib/libpoolalloc.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libaa-test.so \
		$(AA) -aa-test \
		< $<

.PHONY: run
