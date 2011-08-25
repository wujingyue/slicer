# Author: Jingyue

PROG_NAMES = test-thread test-overwrite test-loop \
	     test-reducer test-pointer test-range-2 \
	     test-recursive test-bound FFT-like \
	     aget-like test-array test-dep \
	     test-overwrite-2 RADIX-like test-thread-2 \
	     test-malloc test-ctxt test-range test-range-3 test-range-4 \
	     aget FFT RADIX pbzip2 CHOLESKY pfscan
PROGS_DIR = ../progs
TRACED_PROGS = $(PROG_NAMES:=.trace)
LANDMARK_TRACES = $(PROG_NAMES:=.lt)
FULL_TRACES = $(PROG_NAMES:=.ft)

all: $(TRACED_PROGS)

landmark-trace: $(LANDMARK_TRACES)

full-trace: $(FULL_TRACES)

%.bc1: $(PROGS_DIR)/%.id.bc
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

# Command lines to generate full traces
test-range.ft: test-range.trace
	./$< 2 10
	mv /tmp/fulltrace $@

test-range-2.ft: test-range-2.trace
	./$< 2 10
	mv /tmp/fulltrace $@

test-range-3.ft: test-range-3.trace
	./$< 2 10
	mv /tmp/fulltrace $@

test-dep.ft: test-dep.trace
	./$< 100
	mv /tmp/fulltrace $@

test-reducer.ft: test-reducer.trace
	./$< 1 2 3
	mv /tmp/fulltrace $@

test-loop.ft: test-loop.trace
	./$< 3
	mv /tmp/fulltrace $@

test-bound.ft: test-bound.trace
	./$< 5 10 15
	mv /tmp/fulltrace $@

RADIX-like.ft: RADIX-like.trace
	./$< 4 1024 524288
	mv /tmp/fulltrace $@

FFT-like.ft: FFT-like.trace
	./$< 4
	mv /tmp/fulltrace $@

aget-like.ft: aget-like.trace
	./$< 16 4
	mv /tmp/fulltrace $@

aget.ft: aget.trace
	./$< -n2 -f http://www.cs.columbia.edu/~jingyue/index.html
	# ./$< -n8 -f http://www.cs.columbia.edu/~heming/unlink/dragonegg-2.7-bak0.tgz
	mv /tmp/fulltrace $@

pbzip2.ft: pbzip2.trace
	./$< -p8 -dkvf ../progs/inputs/pbzip2-test.bz2
	mv /tmp/fulltrace $@

CHOLESKY.ft: CHOLESKY.trace
	./$< -p2 < ../progs/inputs/CHOLESKY-lshp.O
	mv /tmp/fulltrace $@

RADIX.ft: RADIX.trace
	./$< -p2 -r1024
	mv /tmp/fulltrace $@

FFT.ft: FFT.trace
	./$< -p4 -m10
	mv /tmp/fulltrace $@

pfscan.ft: pfscan.trace
	# pfscan returns the number of matches
	./$< -n2 -v -d pthread $(LLVM_ROOT)/common/include/ || true
	mv /tmp/fulltrace $@

# Most programs don't need any special command line. 
%.ft: %.trace
	./$<
	mv /tmp/fulltrace $@

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
		< $(PROGS_DIR)/$(<:.ft=.id.bc)

%.landmarks: $(PROGS_DIR)/%.id.bc
	opt -stats -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-mark-landmarks \
		< $<

clean:
	rm -f *.trace *.trace.bc *.trace.s *.bc1 *.ft *.lt

.PHONY: clean full-trace landmark-trace *.landmarks
