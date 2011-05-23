ORIG_PROGS=test-loop.llvm test-thread.llvm
SLICED_PROGS=$(ORIG_PROGS:.llvm=.slice.llvm)
PROGS=$(ORIG_PROGS) $(SLICED_PROGS)

CFLAGS=-g -O2 -Wall
CXXFLAGS=-g -O2 -Wall

all: $(PROGS:.llvm=.ll) $(PROGS)

%.bc: %.c
	llvm-gcc -o $@ $< -c -emit-llvm $(CFLAGS)

%.bc: %.cpp
	llvm-g++ -o $@ $< -c -emit-llvm $(CXXFLAGS)

%.s: %.bc
	llc $< -o $@

%: %.s
	g++ $< -o $@

%.trace: %.trace.s
	g++ $< -o $@ -pthread

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

%.llvm: %.s
	$(CXX) -o $@ $< $(CXXFLAGS)

test-thread.llvm: test-thread.s
	$(CXX) -o $@ $< $(CXXFLAGS) -lpthread

test-thread.slice.llvm: test-thread.slice.s
	$(CXX) -o $@ $< $(CXXFLAGS) -lpthread

test-dual.llvm: test-dual.s
	$(CXX) -o $@ $< $(CXXFLAGS) -lpthread

test-dual.slice.llvm: test-dual.slice.s
	$(CXX) -o $@ $< $(CXXFLAGS) -lpthread

clean:
	rm -rf *.id *.ll *.loop *.bc1 *.trace.bc *.s $(PROGS)
