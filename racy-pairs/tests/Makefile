all: test-race.orig-rp test-race.cloned-rp

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
		-load $(LLVM_ROOT)/install/lib/libmax-slicing-unroll.so \
		-max-slicing-unroll \
		-trace $(<:.bc=.trace) -cut $(<:.bc=.cut) < $<

%.map: %.bc %.trace %.cut
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libmay-exec.so \
		-load $(LLVM_ROOT)/install/lib/libmax-slicing-unroll.so \
		-max-slicing-unroll \
		-trace $(<:.bc=.trace) -cut $(<:.bc=.cut) \
		< $< > $@

%.orig-rp: %.slice.bc %.trace %.map
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libracy-pairs.so \
		-trace $(<:.slice.bc=.trace) \
		-clone-map $(<:.slice.bc=.map) \
		-racy-pairs < $< > $@

%.cloned-rp: %.slice.bc %.trace %.map
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libracy-pairs.so \
		-trace $(<:.slice.bc=.trace) \
		-clone-map $(<:.slice.bc=.map) \
		-racy-pairs -cloned < $< > $@

%.ll: %.bc
	llvm-dis $<

clean:
	rm -rf *.orig-rp *.cloned-rp *.ll

