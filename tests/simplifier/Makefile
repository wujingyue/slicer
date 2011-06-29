# simplifier only works on sliced programs 

PROG_NAMES = aget-nocrit test-branch
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
%.simple.bc: %.slice.bc
	simplifier -o $@ < $<

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
