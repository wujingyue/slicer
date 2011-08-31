LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj
LIBRARYNAME = int-test
LOADABLE_MODULE = 1
CXXFLAGS += -I$(LLVM_ROOT) -I$(SLICER_ROOT) -I$(SLICER_ROOT)/stp/install/include

include $(LEVEL)/Makefile.common

PROG_NAMES = test-overwrite test-loop test-loop-2 \
	     test-reducer \
	     test-bound FFT-like \
	     test-thread \
	     test-array aget-like \
	     test-overwrite-2 \
	     test-thread-2 RADIX-like \
	     test-malloc test-range \
	     aget FFT
PROGS_DIR = ../progs

run:: $(PROG_NAMES)

%: $(PROGS_DIR)/%.simple.bc ../trace/%.lt
	opt -stats -disable-output \
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
		-input-landmark-trace $(word 2, $^) \
		< $<

%.ctxt: $(PROGS_DIR)/%.simple.bc
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-load $(LLVM_ROOT)/install/lib/libmax-slicing.so \
		-load $(LLVM_ROOT)/install/lib/libint.so \
		-count-ctxts \
		< $<

%.ic: $(PROGS_DIR)/%.simple.bc ../trace/%.lt
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
		-input-landmark-trace $(word 2, $^) \
		< $< 2> $@

clean::
	rm -f *.ic *.ctxt

.PHONY: run clean
