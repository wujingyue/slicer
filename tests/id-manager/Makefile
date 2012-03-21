PROG_NAMES = test-thread test-overwrite test-loop test-ctxt-2 test-loop-3 \
	     test-reducer test-recursive test-ret test-ctxt-3 test-ctxt-4 \
	     test-phi test-bound FFT-like test-dep test-loop-4 test-loop-5 \
	     aget-like test-array test-pointer test-global raytrace-like \
	     test-overwrite-2 RADIX-like test-thread-2 pbzip2-like \
	     test-malloc test-ctxt test-range test-barrier ferret-like \
	     test-range-2 test-range-3 test-range-4 test-alloca \
	     test-loop-2 test-assert test-lcssa test-path-2 test-lcssa-2 \
	     bodytrack-like \
	     aget FFT RADIX pbzip2 CHOLESKY LU-cont blackscholes
PROGS_DIR = ../progs
PROGS = $(addprefix $(PROGS_DIR)/, $(PROG_NAMES))
BCS = $(addsuffix .bc, $(PROGS))
TAGGED_BCS = $(addsuffix .id.bc, $(PROGS))

all: $(TAGGED_BCS)

%.id.bc: %.prep.bc
	opt -o $@ \
		-load $(LLVM_ROOT)/install/lib/id.so \
		-tag-id \
		< $<
clean:
	rm -f $(TAGGED_BCS)

.PHONY: clean
