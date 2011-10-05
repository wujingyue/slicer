LEVEL = ../..

LIBRARYNAME = int

LOADABLE_MODULE = 1
BUILD_ARCHIVE = 1

# Include $(LLVM_ROOT) only for bc2bdd
CXXFLAGS += -I$(LLVM_ROOT) -I$(LLVM_ROOT)/install/include -I$(SLICER_ROOT)/lib/stp/install/include
LDFLAGS += -L$(SLICER_ROOT)/lib/stp/install/lib

include $(LEVEL)/Makefile.common

# Must be put after the include. 
LIBS += -lstp
