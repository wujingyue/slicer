PROGS = test-thread test-path test-overwrite test-ctxt test-recursive \
	test-branch test-loop test-reducer test-bound test-ret test-phi \
	aget-like test-array test-pointer test-overwrite-2 test-loop-3 \
	FFT-like RADIX-like test-thread-2 test-malloc test-array-2 test-array-3 \
	test-switch test-range test-dep test-range-2 test-range-3 ferret-like \
	test-range-4 test-loop-2 test-ctxt-2 test-ctxt-3 test-loop-4 \
	test-ctxt-4 test-lcssa test-barrier test-path-2 test-lcssa-2 \
	test-no-slice test-global test-assert test-alloca pbzip2-like \
	raytrace-like test-loop-5 bodytrack-like
BCS = $(PROGS:=.bc)

# Better not use -O here; otherwise difficult to construct examples.
CFLAGS = -g -Wall
CXXFLAGS = -g -Wall

# Generating executables is optional.
all: $(BCS)

%.bc: %.c
	clang $< -o $@ -c -emit-llvm $(CFLAGS)

%.bc: %.cpp
	clang++ $< -o $@ -c -emit-llvm $(CXXFLAGS)

# pthread may not always be necessary, but does no harm
# Also, most of our test programs are multi-threaded
%: %.bc
	clang++ $< -o $@ -pthread

# Cancel the implicit rules from *.c to *
%: %.c
# Cancel the implicit rules from *.cpp to *
%: %.cpp

# Utilities
# Instruction ID using IDManager
%.iid: %.bc
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/id.so \
		-manage-id \
		< $< > $@
# Instruction ID
%.id: %.bc
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/id.so \
		-assign-id -print-insts \
		< $< > $@
# Value ID
%.vid: %.bc
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/id.so \
		-assign-id -print-values \
		< $< > $@

# Call graph
%.cg: %.bc
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/id.so \
		-load $(LLVM_ROOT)/install/lib/bc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/cfg.so \
		-callgraph-fp \
		< $<

# LLVM disassembly
%.ll: %.bc
	llvm-dis $<
# Instruction ID <==> File: Line
%.loc: %.bc
	opt -disable-output \
		-load $(LLVM_ROOT)/install/lib/id.so \
		-load $(LLVM_ROOT)/install/lib/src-locator.so \
		-src-locator \
		-input $(ARG) \
		< $<

clean:
	rm -f *.iid *.id *.mid *.vid *.ll *.loc *.s $(BCS) $(PROGS)

.PHONY: clean
