ifndef SAMPLE
SAMPLE = 1
endif

all: test-race.orig-rp test-race.cloned-rp test-context.orig-rp \
	test-context.cloned-rp

%.bc: %.c
	llvm-gcc -o $@ $< -c -emit-llvm

%.bc: %.cpp
	llvm-g++ -o $@ $< -c -emit-llvm

%.id: %.bc
	$(LLVM_ROOT)/scripts/print-id insts $< > $@

%.slice.bc: %.bc %.trace %.cut
	opt -o $@ \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libmay-exec.so \
		-load $(LLVM_ROOT)/install/lib/libtern-trace.so \
		-load $(LLVM_ROOT)/install/lib/libmax-slicing-unroll.so \
		-max-slicing-unroll \
		-fulltrace $(<:.bc=.ft) \
		-trace $(<:.bc=.trace) -cut $(<:.bc=.cut) \
		-mapping $(<:.bc=.map) < $<

%.orig-rp: %.bc %.trace
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libtern-trace.so \
		-load $(LLVM_ROOT)/install/lib/libracy-pairs.so \
		-fulltrace $(<:.bc=.ft) \
		-trace $(<:.bc=.trace) \
		-sample $(SAMPLE) \
		-racy-pairs < $< > $@

%.cloned-rp: %.slice.bc %.trace
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libtern-trace.so \
		-load $(LLVM_ROOT)/install/lib/libracy-pairs.so \
		-fulltrace $(<:.bc=.ft) \
		-trace $(<:.slice.bc=.trace) \
		-sample $(SAMPLE) \
		-racy-pairs < $< > $@

%.ll: %.bc
	llvm-dis $<

clean:
	rm -rf *.orig-rp *.cloned-rp *.ll

