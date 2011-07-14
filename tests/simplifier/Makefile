# simplifier only works on sliced programs 

PROG_NAMES = aget-nocrit test-branch FFT-nocrit RADIX-nocrit test-loop \
	     test-reducer
PROGS_DIR = ../progs
PROGS = $(addprefix $(PROGS_DIR)/, $(PROG_NAMES))
SIMPLE_PROGS = $(PROGS:=.simple)
SIMPLE_BCS = $(SIMPLE_PROGS:=.bc)

# We don't build simplified executables by default. 
all: $(SIMPLE_BCS)

# If the sliced program exists, work on it. 
# Otherwise, work on the original program. 
# We use -o whenever possible, because <simplifier> will remove the output
# file on failure. 
$(PROGS_DIR)/%.simple.bc: $(PROGS_DIR)/%.slice.bc ../trace/%.lt
	simplifier -o $@ -input-landmark-trace $(word 2, $^) < $<

%.slice.bc: %.bc
	cp $< $@

%.s: %.bc
	llc $< -o $@
# pthread may not always be necessary, but does no harm
# Also, most of our test programs are multi-threaded
%: %.s
	$(CXX) $< -o $@ -pthread

clean:
	rm -f $(SIMPLE_BCS) $(SIMPE_PROGS)

.PHONY: clean
