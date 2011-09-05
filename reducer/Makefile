LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj
LIBRARYNAME = reducer
LOADABLE_MODULE = 1
CXXFLAGS += -I$(LLVM_ROOT) -I$(LLVM_ROOT)/install/include -I$(SLICER_ROOT) -I$(SLICER_ROOT)/stp/install/include
include $(LEVEL)/Makefile.common
