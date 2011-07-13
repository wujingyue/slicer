LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj
LIBRARYNAME = int-test
LOADABLE_MODULE = 1
SOURCES = int-test.cpp
include $(LEVEL)/Makefile.common

CXXFLAGS += -I$(LLVM_ROOT) -I$(SLICER_ROOT) -I$(SLICER_ROOT)/stp/install/include

PROG_NAMES = aget-nocrit.simple test-overwrite.slice FFT-nocrit.slice \
	     FFT-nocrit.simple RADIX-nocrit.simple test-loop.slice \
	     test-loop.simple
PROGS_DIR = ../progs

run:: $(PROG_NAMES)

landmark_trace_name = $(subst .simple,,$(subst .slice,,$(1))).lt

%: $(PROGS_DIR)/%.bc
	opt -disable-output \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-load $(LLVM_ROOT)/install/lib/libmax-slicing.so \
		-load $(LLVM_ROOT)/install/lib/libint.so \
		-load $(LLVM_ROOT)/install/lib/libint-test.so \
		-int-test \
		-prog $@ \
		-input-landmark-trace ../trace/$(call landmark_trace_name,$@) \
		< $<

%.ic: $(PROGS_DIR)/%.bc
	opt -disable-output \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-load $(LLVM_ROOT)/install/lib/libmax-slicing.so \
		-load $(LLVM_ROOT)/install/lib/libint.so \
		-load $(LLVM_ROOT)/install/lib/libint-test.so \
		-int-test \
		-input-landmark-trace \
		../trace/$(call landmark_trace_name,$(@:.ic=)) \
		< $< 2> $@

.PHONY: run
