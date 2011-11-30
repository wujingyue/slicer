# simplifier only works on sliced programs 

PROG_NAMES = test-loop test-reducer test-overwrite test-loop-2 test-ctxt-2 \
	     test-bound FFT-like test-dep test-ctxt-4 \
	     aget-like test-array test-thread test-assert \
	     RADIX-like test-thread-2 test-range-2 \
	     test-overwrite-2 test-malloc test-range \
	     test-range-3 test-range-4 test-global \
	     aget RADIX FFT LU-cont blackscholes
PROGS_DIR = ../progs
PROGS = $(addprefix $(PROGS_DIR)/, $(PROG_NAMES))
SIMPLE_PROGS = $(PROGS:=.simple)
SIMPLE_BCS = $(SIMPLE_PROGS:=.bc)

# We don't build simplified executables by default. 
all: $(SIMPLE_BCS)

$(PROGS_DIR)/test-overwrite.simple.bc: $(PROGS_DIR)/test-overwrite.slice.bc
	simplifier -max-iter=0 -o $@ < $<

$(PROGS_DIR)/test-overwrite-2.simple.bc: $(PROGS_DIR)/test-overwrite-2.slice.bc
	simplifier -max-iter=0 -o $@ < $<

$(PROGS_DIR)/test-bound.simple.bc: $(PROGS_DIR)/test-bound.slice.bc
	simplifier -max-iter=0 -o $@ < $<

$(PROGS_DIR)/test-thread.simple.bc: $(PROGS_DIR)/test-thread.slice.bc
	simplifier -max-iter=0 -o $@ < $<

$(PROGS_DIR)/test-array.simple.bc: $(PROGS_DIR)/test-array.slice.bc
	simplifier -max-iter=0 -o $@ < $<

$(PROGS_DIR)/test-malloc.simple.bc: $(PROGS_DIR)/test-malloc.slice.bc
	simplifier -max-iter=0 -o $@ < $<

$(PROGS_DIR)/test-range-4.simple.bc: $(PROGS_DIR)/test-range-4.slice.bc
	simplifier -max-iter=0 -o $@ < $<

$(PROGS_DIR)/test-loop-2.simple.bc: $(PROGS_DIR)/test-loop-2.slice.bc
	simplifier -max-iter=0 -o $@ < $<

$(PROGS_DIR)/test-ctxt-2.simple.bc: $(PROGS_DIR)/test-ctxt-2.slice.bc
	simplifier -max-iter=0 -o $@ < $<

# We use -o whenever possible, because <simplifier> will remove the output
# file on failure. 
# For aget, need turn on flag unit-at-a-time to disable some annoying LLVM opts. 
$(PROGS_DIR)/aget.simple.bc: $(PROGS_DIR)/aget.slice.bc ../trace/aget.lt
	simplifier -o $@ -input-landmark-trace $(word 2, $^) -p -funit-at-a-time=false < $<

$(PROGS_DIR)/%.simple.bc: $(PROGS_DIR)/%.slice.bc ../trace/%.lt
	simplifier -o $@ -input-landmark-trace $(word 2, $^) -p < $<

%.s: %.bc
	llc $< -o $@
# pthread may not always be necessary, but does no harm
# Also, most of our test programs are multi-threaded
%: %.s
	$(CXX) $< -o $@ -pthread

# LLVM disassembly
%.ll: %.bc
	llvm-dis $<

%.loop: %.bc
	opt -stats -analyze \
		-load $(LLVM_ROOT)/install/lib/id.so \
		-load $(LLVM_ROOT)/install/lib/bc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/cfg.so \
		-load $(LLVM_ROOT)/install/lib/slicer-trace.so \
		-load $(LLVM_ROOT)/install/lib/max-slicing.so \
		-load $(LLVM_ROOT)/install/lib/int.so \
		-load $(LLVM_ROOT)/install/lib/misc.so \
		-identify-loops \
		< $<

clean:
	rm -f $(SIMPLE_BCS) $(SIMPE_PROGS)

.PHONY: clean *.loop
