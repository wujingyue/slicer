PROG_NAMES = test-loop test-reducer test-thread test-overwrite test-recursive \
	     test-bound FFT-like aget-like \
	     FFT-nocrit RADIX-nocrit aget-nocrit pbzip2-nocrit
PROGS_DIR = ../progs
PROGS = $(addprefix $(PROGS_DIR)/, $(PROG_NAMES))
SLICED_PROGS = $(addsuffix .slice, $(PROGS))
SLICED_BCS = $(SLICED_PROGS:=.bc)

# We don't SLICED_PROGS by default. 
all: $(SLICED_BCS)

region: $(addsuffix .region, $(PROG_NAMES))

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

# These BC's are not used by other modules, therefore put them here locally. 
%.region: $(PROGS_DIR)/%.slice.bc ../trace/%.lt
	opt -disable-output \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-load $(LLVM_ROOT)/install/lib/libmax-slicing.so \
		-manage-region \
		-input-landmark-trace $(word 2, $^) \
		< $<

%.ll: %.bc
	llvm-dis $<

%.s: %.bc
	llc $< -o $@
# pthread may not always be necessary, but does no harm
# Also, most of our test programs are multi-threaded
%: %.s
	$(CXX) $< -o $@ -pthread

clean:
	rm -f $(SLICED_PROGS) $(SLICED_BCS)

.PHONY: clean region *.region
