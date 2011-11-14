LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj
LIBRARYNAME = icfg-test
LOADABLE_MODULE = 1
SOURCES = icfg-test.cpp
include $(LEVEL)/Makefile.common

CXXFLAGS += -I$(LLVM_ROOT)

PROG_NAMES = aget aget-nocrit test-thread test-overwrite
PROGS_DIR = ../progs

run:: $(addsuffix .slice, $(PROG_NAMES))

%.icfg: $(PROGS_DIR)/%.bc
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/idm.so \
		-load $(LLVM_ROOT)/install/lib/bc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/callgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/cfg.so \
		-load $(LLVM_ROOT)/install/lib/icfg-test.so \
		-icfg-test \
		< $< > $@

%: $(PROGS_DIR)/%.bc
	opt -disable-output \
		-load $(LLVM_ROOT)/install/lib/idm.so \
		-load $(LLVM_ROOT)/install/lib/bc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/callgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/cfg.so \
		-load $(LLVM_ROOT)/install/lib/icfg-test.so \
		-icfg-test -prog $@ < $<

clean::
	rm -f *.icfg

.PHONY: clean
