CFLAGS=-g -O2
CXXFLAGS=-g -O2

all: test-race.bc test-context.bc test-gep.bc test-int.bc test-recursive.bc

%.bc: %.c
	llvm-gcc -o $@ $< -c -emit-llvm $(CFLAGS)

%.bc: %.cpp
	llvm-g++ -o $@ $< -c -emit-llvm $(CXXFLAGS)

%.id: %.bc
	$(LLVM_ROOT)/scripts/print-id insts $< > $@

%.ic: %.bc
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libint-constraints.so \
		-capture-constraints \
		< $< > $@

%.eo: %.bc
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libint-constraints.so \
		-exec-once \
		< $< > $@

%.ma: %.bc
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libcallgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/libint-constraints.so \
		-must-alias \
		< $< > $@

%.ll: %.bc
	llvm-dis $<

clean:
	rm -rf *.id *.ll

