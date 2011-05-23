ORIG_PROGS=test-thread
SLICED_PROGS=$(ORIG_PROGS>.slice)
PROGS=$(ORIG_PROGS) $(SLICED_PROGS)

CFLAGS=-g -O2 -Wall
CXXFLAGS=-g -O2 -Wall

all: $(PROGS:>.ll) $(PROGS)

%.bc: %.c
	llvm-gcc -o $@ $< -c -emit-llvm $(CFLAGS)

%.bc: %.cpp
	llvm-g++ -o $@ $< -c -emit-llvm $(CXXFLAGS)

%.s: %.bc
	llc $< -o $@

# pthread may not always be necessary, but does no harm
# Also, most of our test programs are multi-threaded
%: %.s
	$(CXX) $< -o $@ -pthread

# Cancel the implicit rules from *.c to *
%: %.c

# Cancel the implicit rules from *.cpp to *
%: %.cpp

%.id: %.bc
	$(LLVM_ROOT)/scripts/print-id insts $< > $@

%.ll: %.bc
	llvm-dis $<

%.loop: %.bc
	opt -disable-output \
		-load $(LLVM_ROOT)/install/lib/libidentify-loops.so \
		-identify-loops \
		< $<

%.bc1: %.bc
	opt \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libmay-exec.so \
		-load $(LLVM_ROOT)/install/lib/libidentify-thread-funcs.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-instrument \
		< $< > $@

%.display: %.ft
	../trace/display/display $<

%.slice.bc: %.bc %.ft
	opt -o $@ \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libmay-exec.so \
		-load $(LLVM_ROOT)/install/lib/libidentify-thread-funcs.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-load $(LLVM_ROOT)/install/lib/libmax-slicing-unroll.so \
		-max-slicing-unroll \
		-fulltrace $(<:.bc=.ft) \
		-mapping $(<:.bc=.map) < $<

%.lt: %.bc %.ft
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libmay-exec.so \
		-load $(LLVM_ROOT)/install/lib/libidentify-thread-funcs.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-landmark-trace \
		-fulltrace $(<:.bc=.ft) \
		< $<

%.ml: %.bc %.ft
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libmay-exec.so \
		-load $(LLVM_ROOT)/install/lib/libidentify-thread-funcs.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-mark-landmarks \
		-fulltrace $(<:.bc=.ft) \
		< $<

%.trace.bc: %.bc1 ../trace/tracing/tracing.bc
	llvm-ld $^ -b $@

clean:
	rm -rf *.id *.ll *.loop *.bc1 *.trace.bc *.s $(PROGS)
