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
		-load $(LLVM_ROOT)/install/lib/idm.so \
		-load $(LLVM_ROOT)/install/lib/id-manager.so \
		-load $(LLVM_ROOT)/install/lib/bc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/LLVMDataStructure.so \
		-load $(LLVM_ROOT)/install/lib/poolalloc.so \
		-load $(LLVM_ROOT)/install/lib/callgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/cfg.so \
		-load $(LLVM_ROOT)/install/lib/aa-test.so \
		$(AA) -aa-test \
		< $<

.PHONY: run
