PROG_NAMES = test-thread test-path test-overwrite test-ctxt test-recursive \
	     test-branch test-loop test-reducer test-bound test-ret test-phi \
	     FFT-like aget-like test-array test-pointer test-overwrite-2 \
	     RADIX-like test-thread-2 test-malloc test-array-2 test-array-3 \
	     test-range test-dep test-range-2 test-range-3 test-range-4 \
	     test-loop-2 test-ctxt-2 \
	     aget FFT RADIX pbzip2 CHOLESKY pfscan
PROGS_DIR = ../progs

PROGS = $(addprefix $(PROGS_DIR)/, $(PROG_NAMES))

all: $(PROGS:=.prep.bc)

# Use -o when possible
# <opt> will remove the output file on failure.
%.prep.bc: %.bc
	opt -o $@ \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libpreparer.so \
		-break-crit-edges -prepare \
		< $<

clean:
	rm -f $(PROGS:=.prep.bc)

.PHONY: clean
