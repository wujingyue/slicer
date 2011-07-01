LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj
LIBRARYNAME = icfg-test
LOADABLE_MODULE = 1
SOURCES = icfg-test.cpp
include $(LEVEL)/Makefile.common

CXXFLAGS += -I$(LLVM_ROOT)

PROG_NAMES = aget aget-nocrit test-thread test-overwrite
PROGS_DIR = ../progs

run:: $(addsuffix .slice.icfg, $(PROG_NAMES))

%.icfg: $(PROGS_DIR)/%.bc
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libicfg-test.so \
		-icfg-test \
		< $< > $@

%: $(PROGS_DIR)/%.bc
	opt -disable-output \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libicfg-test.so \
		-icfg-test -prog $@ < $<

clean::
	rm -f *.icfg

.PHONY: clean
