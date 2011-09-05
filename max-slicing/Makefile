LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj

LIBRARYNAME = max-slicing

LOADABLE_MODULE = 1

CXXFLAGS += -I$(LLVM_ROOT) -I$(SLICER_ROOT) -I$(LLVM_ROOT)/install/include

include $(LEVEL)/Makefile.common
