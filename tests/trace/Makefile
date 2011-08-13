PROG_NAMES = test-thread-nocrit test-overwrite-nocrit test-loop-nocrit \
	     test-reducer-nocrit test-pointer-nocrit \
	     test-recursive-nocrit test-bound-nocrit FFT-like-nocrit \
	     aget-like-nocrit test-array-nocrit test-dep-nocrit \
	     test-overwrite-2-nocrit RADIX-like-nocrit test-thread-2-nocrit \
	     test-malloc-nocrit test-ctxt-nocrit test-range-nocrit \
	     aget-nocrit FFT-nocrit RADIX-nocrit \
	     pbzip2-nocrit CHOLESKY-nocrit
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
%.s: %.bc
	llc $< -o $@ -O0

# pthread may not always be necessary, but does no harm
# Also, most of our test programs are multi-threaded
%: %.s
	$(CXX) $< -o $@ -pthread

pbzip2%: pbzip2%.s
	$(CXX) $< -o $@ -pthread -lbz2

# ./test-reducer-nocrit 1 2 3
# ./test-loop-nocrit 3
# ./test-bound-nocrit 5 10 15
# ./RADIX-nocrit -p2 -r1024
# ./FFT-nocrit -p4 -m10
# ./RADIX-like 4 1024 524288
# ./FFT-like 4
# ./aget-like 16 4
# ./aget-nocrit -n2 -f http://www.cs.columbia.edu/~jingyue/index.html
# ./pbzip2-nocrit -p8 -dkvf test.bz2
# ./CHOLESKY -p2 < lshp.O

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
	display-trace full < $<

%.display-lt: %.lt
	display-trace landmark < $<

%.landmarks: $(PROGS_DIR)/%-id.bc
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-mark-landmarks \
		< $<

clean:
	rm -f *.trace *.trace.bc *.trace.s *.bc1 *.lt

.PHONY: clean landmark-trace *.display-lt *.display-ft *.landmarks
