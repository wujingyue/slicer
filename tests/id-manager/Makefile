PROG_NAMES = test-thread test-path test-overwrite test-ctxt test-recursive \
	test-branch test-loop \
	aget aget-nocrit 
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
