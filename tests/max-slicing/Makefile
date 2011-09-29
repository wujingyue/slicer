PROG_NAMES = test-loop test-reducer test-thread test-loop-2 test-ctxt-2 \
	     test-overwrite test-recursive test-dep test-loop-3 \
	     test-bound FFT-like aget-like test-ctxt-3 test-loop-4 \
	     test-array test-pointer test-range-2 test-ctxt-4 \
	     test-overwrite-2 RADIX-like test-thread-2 test-global \
	     test-malloc test-ctxt test-range \
	     test-range-3 test-range-4 \
	     FFT RADIX aget pbzip2 CHOLESKY
PROGS_DIR = ../progs
PROG_PATHS = $(addprefix $(PROGS_DIR)/, $(PROG_NAMES))
SLICED_PROGS = $(addsuffix .slice, $(PROG_PATHS))
SLICED_BCS = $(SLICED_PROGS:=.bc)

# We don't SLICED_PROGS by default. 
all: $(SLICED_BCS)

region: $(addsuffix .region, $(PROG_NAMES))

# max-slicing requires ID information. 
$(PROGS_DIR)/%.slice.bc: $(PROGS_DIR)/%.id.bc ../trace/%.lt
	opt -stats -o $@ \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libmbb.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-load $(LLVM_ROOT)/install/lib/libmax-slicing.so \
		-max-slicing \
		-input-landmark-trace $(word 2, $^) \
		< $<

# These BC's are not used by other modules, therefore put them here locally. 
%.region: $(PROGS_DIR)/%.slice.bc ../trace/%.lt
	opt -disable-output \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libmbb.so \
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
