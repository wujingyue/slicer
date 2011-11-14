LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj
LIBRARYNAME = passes-test
LOADABLE_MODULE = 1
include $(LEVEL)/Makefile.common

CXXFLAGS += -I$(LLVM_ROOT)

# The input can be any bc file. 
run::
	opt -disable-output \
		-load $(LLVM_ROOT)/install/lib/passes-test.so \
		-passes-test \
		< ../progs/test-branch.bc

.PHONY: run
