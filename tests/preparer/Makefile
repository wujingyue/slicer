PROG_NAMES = test-thread test-path test-overwrite test-ctxt test-recursive \
	     test-branch test-loop test-reducer test-bound test-ret test-phi \
	     FFT-like aget-like test-array test-pointer test-overwrite-2 \
	     RADIX-like test-thread-2 test-malloc test-array-2 test-array-3 \
	     test-range test-dep test-range-2 test-range-3 test-range-4 \
	     test-loop-2 test-ctxt-2 test-loop-3 test-ctxt-3 test-loop-4 \
	     test-assert test-ctxt-4 test-global \
	     aget FFT RADIX pbzip2 CHOLESKY pfscan aget-fake LU-cont \
	     swaptions streamcluster blackscholes
PROGS_DIR = ../progs

PROGS = $(addprefix $(PROGS_DIR)/, $(PROG_NAMES))

all: $(PROGS:=.prep.bc)

# Use -o when possible
# <opt> will remove the output file on failure.
%.prep.bc: %.bc
	opt -o $@ \
		-load $(LLVM_ROOT)/install/lib/id-manager.so \
		-load $(LLVM_ROOT)/install/lib/bc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/callgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/mbb.so \
		-load $(LLVM_ROOT)/install/lib/cfg.so \
		-load $(LLVM_ROOT)/install/lib/preparer.so \
		-prepare \
		< $<

clean:
	rm -f $(PROGS:=.prep.bc)

.PHONY: clean
