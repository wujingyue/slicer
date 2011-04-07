CFLAGS=-g -Wall
CXXFLAGS=-g -Wall

ORIG_PROGS=test.llvm test-skip.llvm test-loop.llvm test-sync-loop.llvm \
	   test-thread.llvm test-dual.llvm test-sync-br.llvm

SLICED_PROGS=$(ORIG_PROGS:.llvm=.slice.llvm)

PROGS=$(ORIG_PROGS) $(SLICED_PROGS)

all: $(PROGS:.llvm=.ll) $(PROGS)

%.ll: %.bc
	llvm-dis $<

%.bc: %.c
	llvm-gcc -o $@ $< -c -emit-llvm

%.bc: %.cpp
	llvm-g++ -o $@ $< -c -emit-llvm

%.id: %.bc
	$(LLVM_ROOT)/scripts/print-id insts $< > $@

%.slice.bc: %.bc
	opt -o $@ \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libmay-exec.so \
		-load $(LLVM_ROOT)/install/lib/libidentify-thread-funcs.so \
		-load $(LLVM_ROOT)/install/lib/libtern-trace.so \
		-load $(LLVM_ROOT)/install/lib/libmax-slicing-unroll.so \
		-max-slicing-unroll \
		-fulltrace $(<:.bc=.ft) \
		-mapping $(<:.bc=.map) < $<

test.slice.bc: test.bc test.cut
	opt -o $@ \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libmay-exec.so \
		-load $(LLVM_ROOT)/install/lib/libidentify-thread-funcs.so \
		-load $(LLVM_ROOT)/install/lib/libtern-trace.so \
		-load $(LLVM_ROOT)/install/lib/libmax-slicing-unroll.so \
		-max-slicing-unroll \
		-fulltrace $(<:.bc=.ft) \
		-cut $(<:.bc=.cut) \
		-mapping $(<:.bc=.map) < $<

%.trace: %.bc %.sched %.landmark
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libtern-translate-trace.so \
		-tern-translate-trace \
		-sched $(<:.bc=.sched) -landmark $(<:.bc=.landmark) \
		-trace $@ < $< 

%.cut: %.bc %.sched %.landmark
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libtern-translate-trace.so \
		-tern-translate-trace \
		-sched $(<:.bc=.sched) -landmark $(<:.bc=.landmark) \
		-cut $@ < $< 

%.s: %.bc
	llc -o $@ $< -f

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
	rm -f $(PROGS) *.s *.ll *.bc

