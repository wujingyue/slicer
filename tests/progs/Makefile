PROGS = test-thread test-path test-overwrite test-ctxt test-recursive \
	test-branch test-loop test-reducer
BCS = $(PROGS:=.bc)

CFLAGS = -g -Wall
CXXFLAGS = -g -Wall

# Generating executables is optional.
all: $(BCS)

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

# Utilities
# Instruction ID using IDManager
%.iid: %.bc
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-manage-id \
		< $< > $@
# Instruction ID
%.id: %.bc
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-assign-id -print-insts \
		< $< > $@
# Value ID
%.vid: %.bc
	opt -analyze \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-assign-id -print-values \
		< $< > $@
# LLVM disassembly
%.ll: %.bc
	llvm-dis $<
# Instruction ID <==> File: Line
%.loc: %.bc
	opt -disable-output \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libsrc-locator.so \
		-src-locator \
		-input $(ARG) \
		< $<

clean:
	rm -f *.iid *.id *.mid *.vid *.ll *.loc *.s $(BCS) $(PROGS)

.PHONY: clean
