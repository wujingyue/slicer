PROG_NAMES = test-thread test-path test-overwrite test-ctxt test-recursive \
	test-branch test-loop \
	aget aget-nocrit 
PROGS_DIR = ../progs
PROGS = $(addprefix $(PROGS_DIR)/, $(PROG_NAMES))
TRACED_PROGS = $(PROGS:=.trace)

all: $(TRACED_PROGS)

%.bc1: %.bc
	opt -o $@ \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-instrument \
		< $<
# llvm-ld doesn't generate executables if -link-as-library is specified. 
%.trace.bc: %.bc1 ../../trace/tracing.bc
	llvm-ld $^ -b $@ -link-as-library

%.s: %.bc
	llc $< -o $@
# pthread may not always be necessary, but does no harm
# Also, most of our test programs are multi-threaded
%: %.s
	$(CXX) $< -o $@ -pthread

%.display: ../max-slicing/%.ft
	trace-display $<

clean:
	rm -f $(PROGS:=.bc1) $(PROGS:=.trace.bc) $(PROGS:=.trace.s) \
		$(TRACED_PROGS)

.PHONY: clean *.display
