LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj

TOOLNAME = filter-landmark-trace

CXXFLAGS += -I$(LLVM_ROOT) -I$(LLVM_ROOT)/install/include -I$(SLICER_ROOT)

LINK_COMPONENTS := bitreader bitwriter asmparser instrumentation scalaropts ipo

include $(LEVEL)/Makefile.common
