PROG_NAMES = test-thread test-overwrite test-loop test-reducer \
	     test-recursive test-ret test-phi test-bound \
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
