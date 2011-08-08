PROG_NAMES = test-ctxt \
	     aget FFT RADIX pbzip2 CHOLESKY
PROGS_DIR = ../progs

PROGS = $(addprefix $(PROGS_DIR)/, $(PROG_NAMES))

all: $(PROGS:=-nocrit.bc)

# Use -o when possible
# <opt> will remove the output file on failure.
%-nocrit.bc: %.bc
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
	rm -f $(PROGS:=-nocrit.bc)

.PHONY: clean
