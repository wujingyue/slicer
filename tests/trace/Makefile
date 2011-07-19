PROG_NAMES = test-thread test-overwrite aget-nocrit FFT-nocrit RADIX-nocrit \
	     test-loop test-reducer pbzip2-nocrit CHOLESKY-nocrit
PROGS_DIR = ../progs
TRACED_PROGS = $(PROG_NAMES:=.trace)
LANDMARK_TRACES = $(PROG_NAMES:=.lt)

all: $(TRACED_PROGS)

landmark-trace: $(LANDMARK_TRACES)

%.bc1: $(PROGS_DIR)/%-id.bc
	opt -o $@ \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-instrument \
		< $<
# llvm-ld doesn't generate executables if -link-as-library is specified. 
# We don't really need a highly-optimized recorder, so we disable the opts
# to save the compiling time. 
%.trace.bc: %.bc1 ../../trace/tracing.bc
	llvm-ld $^ -b $@ -link-as-library -disable-opt

# Default option is -O2.
%.trace.s: %.trace.bc
	llc $< -o $@ -O0

# pthread may not always be necessary, but does no harm
# Also, most of our test programs are multi-threaded
%.trace: %.trace.s
	$(CXX) $< -o $@ -pthread

pbzip2%: pbzip2%.s
	$(CXX) $< -o $@ -pthread -lbz2

%.lt: %.ft
	opt -disable-output \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-build-landmark-trace \
		-fulltrace $< \
		-output-landmark-trace $@ \
		< $(PROGS_DIR)/$(<:.ft=-id.bc)

%.display-ft: %.ft
	trace-display full < $<

%.display-lt: %.lt
	trace-display landmark < $<

clean:
	rm -f *.trace *.trace.bc *.trace.s *.bc1 *.lt

.PHONY: clean landmark-trace *.display
