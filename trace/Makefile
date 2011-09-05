LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj

LIBRARYNAME = slicer-trace

LOADABLE_MODULE = 1
BUILD_ARCHIVE = 1

SOURCES = add-calling-context.cpp landmark-trace.cpp omit-branch.cpp \
	  trace-manager.cpp instrument.cpp mark-landmarks.cpp \
	  landmark-trace-builder.cpp enforcing-landmarks.cpp

DIRS = display filter

CXXFLAGS += -I$(LLVM_ROOT) -I$(LLVM_ROOT)/install/include -I$(SLICER_ROOT)

include $(LEVEL)/Makefile.common

all:: tracing.bc

tracing.bc: tracing.cpp
	llvm-g++ $< -o $@ -emit-llvm -c -MMD

clean::
	rm -f tracing.bc tracing.d

-include tracing.d
