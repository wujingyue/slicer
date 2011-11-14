%.count: %.bc
	opt -stats -disable-output \
		-load $(LLVM_ROOT)/install/lib/id-manager.so \
		-load $(LLVM_ROOT)/install/lib/bc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/callgraph-fp.so \
		-load $(LLVM_ROOT)/install/lib/mbb.so \
		-load $(LLVM_ROOT)/install/lib/cfg.so \
		-load $(LLVM_ROOT)/install/lib/slicer-trace.so \
		-load $(LLVM_ROOT)/install/lib/max-slicing.so \
		-load $(LLVM_ROOT)/install/lib/inst-counter.so \
		-count-insts \
		< $<

.PHONY: *.count
