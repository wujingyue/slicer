%.count: %.bc
	opt -stats -disable-output \
		-load $(LLVM_ROOT)/install/lib/id.so \
		-load $(LLVM_ROOT)/install/lib/bc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/cfg.so \
		-load $(LLVM_ROOT)/install/lib/slicer-trace.so \
		-load $(LLVM_ROOT)/install/lib/max-slicing.so \
		-load $(LLVM_ROOT)/install/lib/inst-counter.so \
		-count-insts \
		< $<

.PHONY: *.count
