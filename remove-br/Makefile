LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj
LIBRARYNAME = remove-br
LOADABLE_MODULE = 1
include $(LEVEL)/Makefile.common
CXXFLAGS += -I$(LLVM_ROOT) -I$(SLICER_ROOT)/stp/install/include
