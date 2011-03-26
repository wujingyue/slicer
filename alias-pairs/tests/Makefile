all: test.ap

%.bc: %.c
	llvm-gcc -o $@ $< -c -emit-llvm

%.ap: %.bc
	opt \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libalias-pairs.so \
		-alias-pairs -analyze < $< > $@

clean:
	rm -rf *.ap
