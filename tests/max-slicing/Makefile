PROG_NAMES = aget-nocrit test-thread test-overwrite FFT-nocrit RADIX-nocrit \
	     test-loop test-reducer
PROGS_DIR = ../progs
PROGS = $(addprefix $(PROGS_DIR)/, $(PROG_NAMES))
SLICED_PROGS = $(addsuffix .slice, $(PROGS))
SLICED_BCS = $(SLICED_PROGS:=.bc)

# We don't SLICED_PROGS by default. 
all: $(SLICED_BCS)

# max-slicing requires ID information. 
$(PROGS_DIR)/%.slice.bc: $(PROGS_DIR)/%-id.bc ../trace/%.lt
	opt -o $@ \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-load $(LLVM_ROOT)/install/lib/libmax-slicing.so \
		-max-slicing \
		-input-landmark-trace $(word 2, $^) \
		< $<

%.s: %.bc
	llc $< -o $@
# pthread may not always be necessary, but does no harm
# Also, most of our test programs are multi-threaded
%: %.s
	$(CXX) $< -o $@ -pthread

clean:
	rm -f $(SLICED_PROGS) $(SLICED_BCS)

.PHONY: clean
