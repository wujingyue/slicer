LEVEL = ../..

LIBRARYNAME = slicer-trace

LOADABLE_MODULE = 1

SOURCES = landmark-trace.cpp trace.cpp validity-checker.cpp \
	  trace-manager.cpp instrument.cpp mark-landmarks.cpp \
	  landmark-trace-builder.cpp enforcing-landmarks.cpp

include $(LEVEL)/Makefile.common

all:: tracing.bc

tracing.bc: tracing.cpp
	clang++ $< -o $@ -emit-llvm -c -MMD -I$(LEVEL)/include

clean::
	rm -f tracing.bc tracing.d

-include tracing.d
