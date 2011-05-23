CFLAGS=-g -O2
CXXFLAGS=-g -O2

all: test-loop.bc test-thread.bc

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

%.trace.bc: %.bc1 ../trace/tracing/tracing.bc
	llvm-ld $^ -b $@

clean:
	rm -rf *.id *.ll *.loop *.bc1 *.trace.bc *.s
