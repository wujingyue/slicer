LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj
LIBRARYNAME = int-test
LOADABLE_MODULE = 1
SOURCES = int-test.cpp
include $(LEVEL)/Makefile.common

CXXFLAGS += -I$(LLVM_ROOT) -I$(SLICER_ROOT) -I$(SLICER_ROOT)/stp/install/include
