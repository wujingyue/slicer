ORIG_PROGS=test-thread test-path test-overwrite test-ctxt test-recursive
SLICED_PROGS=$(ORIG_PROGS>.slice)
PROGS=$(ORIG_PROGS) $(SLICED_PROGS)

CFLAGS=-g -O2 -Wall
CXXFLAGS=-g -O2 -Wall

all: $(PROGS>.ll) $(PROGS)

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

%.mid: %.bc
	$(LLVM_ROOT)/scripts/print-id mbbs $< > $@

%.vid: %.bc
	$(LLVM_ROOT)/scripts/print-id values $< > $@

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
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
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
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-load $(LLVM_ROOT)/install/lib/libmax-slicing.so \
		-max-slicing \
		-fulltrace $(<:.bc=.ft) \
		-output-clone-map $(@:.bc=.map) \
		-output-cfg $(@:.bc=.cfg) \
		< $<

%.lt: %.bc %.ft
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-landmark-trace \
		-fulltrace $(<:.bc=.ft) \
		< $< > $@

%.ml: %.bc %.ft
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-mark-landmarks \
		-fulltrace $(<:.bc=.ft) \
		< $<

%.tm: %.bc %.ft
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-trace-manager \
		-fulltrace $(<:.bc=.ft) \
		< $< > $@

%.trace.bc: %.bc1 ../trace/tracing/tracing.bc
	llvm-ld $^ -b $@

%.eo: %.bc
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libint-constraints.so \
		-exec-once \
		< $<

%.iter: %.bc
	opt -disable-output -time-passes \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libint-constraints.so \
		-iterate \
		-test \
		< $<

%.loc: %.bc
	opt -disable-output \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libsrc-locator.so \
		-src-locator \
		-input $(ARG) \
		< $<

%.ctxt: %.bc
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libint-constraints.so \
		-count-ctxts \
		< $<

# Inter-procedural dominator tree
%.idt: %.bc 
	opt -disable-output \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libcfg.so \
		-load $(LLVM_ROOT)/install/lib/libslicer-trace.so \
		-load $(LLVM_ROOT)/install/lib/libmax-slicing.so \
		-load $(LLVM_ROOT)/install/lib/libtest-icfg.so \
		-input-cfg $(<:.bc=.cfg) \
		-test-icfg \
		< $<


clean:
	rm -rf *.id *.ll *.loop *.bc1 *.trace.bc *.s $(PROGS)
