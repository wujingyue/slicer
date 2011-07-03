LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj
LIBRARYNAME = slicer-trace
LOADABLE_MODULE = 1
SOURCES = add-calling-context.cpp landmark-trace.cpp omit-branch.cpp \
	  trace-manager.cpp instrument.cpp mark-landmarks.cpp \
	  landmark-trace-builder.cpp
DIRS = display
include $(LEVEL)/Makefile.common

CXXFLAGS += -I$(LLVM_ROOT) -I$(SLICER_ROOT)

all:: tracing.bc

tracing.bc: tracing.cpp
	llvm-g++ $< -o $@ -emit-llvm -c -MMD

clean::
	rm -f tracing.bc tracing.d

-include tracing.d
