LEVEL = ../..

LIBRARYNAME = int-test

LOADABLE_MODULE = 1

include $(LEVEL)/Makefile.common

PROG_NAMES = test-overwrite test-loop test-loop-2 test-ctxt-4 \
	     test-reducer test-global pbzip2-like ferret-like bodytrack-like \
	     test-bound FFT-like \
	     test-thread test-path-2 \
	     test-array aget-like \
	     test-overwrite-2 test-alloca \
	     test-thread-2 RADIX-like \
	     test-malloc test-range \
	     aget blackscholes FFT
PROGS_DIR = ../progs

run:: $(PROG_NAMES)

%: $(PROGS_DIR)/%.simple.bc ../trace/%.lt
	opt -stats -disable-output \
		-load $(LLVM_ROOT)/install/lib/id.so \
		-load $(LLVM_ROOT)/install/lib/bc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/cfg.so \
		-load $(LLVM_ROOT)/install/lib/slicer-trace.so \
		-load $(LLVM_ROOT)/install/lib/max-slicing.so \
		-load $(LLVM_ROOT)/install/lib/int.so \
		-load $(LLVM_ROOT)/install/lib/int-test.so \
		-int-test \
		-prog $@ \
		-input-landmark-trace $(word 2, $^) \
		< $<

%.ctxt: $(PROGS_DIR)/%.simple.bc
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/id.so \
		-load $(LLVM_ROOT)/install/lib/bc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/cfg.so \
		-load $(LLVM_ROOT)/install/lib/slicer-trace.so \
		-load $(LLVM_ROOT)/install/lib/max-slicing.so \
		-load $(LLVM_ROOT)/install/lib/int.so \
		-count-ctxts \
		< $<

# Keep debug-only. Usually when you want to print an ic file, you need more
# detailed information as well. 
%.ic: $(PROGS_DIR)/%.simple.bc ../trace/%.lt
	opt -disable-output -debug-only=int \
		-load $(LLVM_ROOT)/install/lib/id.so \
		-load $(LLVM_ROOT)/install/lib/bc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/cfg.so \
		-load $(LLVM_ROOT)/install/lib/slicer-trace.so \
		-load $(LLVM_ROOT)/install/lib/max-slicing.so \
		-load $(LLVM_ROOT)/install/lib/int.so \
		-load $(LLVM_ROOT)/install/lib/int-test.so \
		-int-test \
		-input-landmark-trace $(word 2, $^) \
		< $< 2> $@

clean::
	rm -f *.ic *.ctxt

.PHONY: run clean
