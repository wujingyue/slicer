# simplifier only works on sliced programs 

PROG_NAMES = test-loop test-reducer test-overwrite test-bound FFT-like \
	     aget-nocrit FFT-nocrit RADIX-nocrit CHOLESKY-nocrit
PROGS_DIR = ../progs
PROGS = $(addprefix $(PROGS_DIR)/, $(PROG_NAMES))
SIMPLE_PROGS = $(PROGS:=.simple)
SIMPLE_BCS = $(SIMPLE_PROGS:=.bc)

# We don't build simplified executables by default. 
all: $(SIMPLE_BCS)

$(PROGS_DIR)/test-overwrite.simple.bc: $(PROGS_DIR)/test-overwrite.slice.bc
	opt -lcssa -loopsimplify -o $@ < $<

$(PROGS_DIR)/test-bound.simple.bc: $(PROGS_DIR)/test-bound.slice.bc
	opt -O3 -lcssa -loopsimplify -o $@ < $<

# If the sliced program exists, work on it. 
# Otherwise, work on the original program. 
# We use -o whenever possible, because <simplifier> will remove the output
# file on failure. 
$(PROGS_DIR)/%.simple.bc: $(PROGS_DIR)/%.slice.bc ../trace/%.lt
	simplifier -debug-only=region-manager -o $@ -input-landmark-trace $(word 2, $^) -p < $<

%.s: %.bc
	llc $< -o $@
# pthread may not always be necessary, but does no harm
# Also, most of our test programs are multi-threaded
%: %.s
	$(CXX) $< -o $@ -pthread

# LLVM disassembly
%.ll: %.bc
	llvm-dis $<

clean:
	rm -f $(SIMPLE_BCS) $(SIMPE_PROGS)

.PHONY: clean
