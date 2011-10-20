LEVEL = ../..

LIBRARYNAME = int

LOADABLE_MODULE = 1
BUILD_ARCHIVE = 1

include $(LEVEL)/Makefile.common

LDFLAGS += -L$(SLICER_ROOT)/lib/stp/install/lib
# Must be put after the include. 
LIBS += -lstp
