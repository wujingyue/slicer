LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj
LIBRARYNAME = query-simulator
LOADABLE_MODULE = 1
CXXFLAGS += -I$(LLVM_ROOT) -I$(SLICER_ROOT) -I$(SLICER_ROOT)/stp/install/include

include $(LEVEL)/Makefile.common

PROGS_DIR = ../progs

%: $(PROGS_DIR)/%.simple.bc ../trace/%.lt
	opt -stats -disable-output \
		-load $(LLVM_ROOT)/install/lib/idm.so \
		-load $(LLVM_ROOT)/install/lib/id-manager.so \
		-load $(LLVM_ROOT)/install/lib/bc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/callgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/cfg.so \
		-load $(LLVM_ROOT)/install/lib/slicer-trace.so \
		-load $(LLVM_ROOT)/install/lib/max-slicing.so \
		-load $(LLVM_ROOT)/install/lib/int.so \
		-load $(LLVM_ROOT)/install/lib/query-simulator.so \
		-simulate-queries \
		-queries $@.queries \
		-input-landmark-trace $(word 2, $^) \
		< $<
