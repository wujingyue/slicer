PROG_NAMES = test-thread-nocrit test-overwrite-nocrit test-loop-nocrit \
	     test-reducer-nocrit test-recursive-nocrit test-ret-nocrit \
	     test-phi-nocrit test-bound-nocrit FFT-like-nocrit \
	     aget-like-nocrit test-array-nocrit test-pointer-nocrit \
	     test-overwrite-2-nocrit RADIX-like-nocrit test-thread-2-nocrit \
	     test-malloc-nocrit test-ctxt-nocrit \
	     aget-nocrit FFT-nocrit RADIX-nocrit \
	     pbzip2-nocrit CHOLESKY-nocrit
PROGS_DIR = ../progs
PROGS = $(addprefix $(PROGS_DIR)/, $(PROG_NAMES))
BCS = $(addsuffix .bc, $(PROGS))
TAGGED_BCS = $(addsuffix -id.bc, $(PROGS))

all: $(TAGGED_BCS)

%-id.bc: %.bc
	opt -o $@ \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-tag-id \
		< $<
clean:
	rm -f $(TAGGED_BCS)

.PHONY: clean
