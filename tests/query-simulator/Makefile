LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj
LIBRARYNAME = query-simulator
LOADABLE_MODULE = 1
CXXFLAGS += -I$(LLVM_ROOT) -I$(SLICER_ROOT) -I$(SLICER_ROOT)/stp/install/include

include $(LEVEL)/Makefile.common

PROGS_DIR = ../progs

%: $(PROGS_DIR)/%.simple.bc ../trace/%.lt
	opt -debug-only=int -stats -disable-output \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-load $(LLVM_ROOT)/install/lib/libmax-slicing.so \
		-load $(LLVM_ROOT)/install/lib/libint.so \
		-load $(LLVM_ROOT)/install/lib/libquery-simulator.so \
		-simulate-queries \
		-queries $@.queries \
		-input-landmark-trace $(word 2, $^) \
		< $<
