all: tracing.bc

tracing.bc: tracing.cpp
	llvm-g++ $< -o $@ -emit-llvm -c -MMD

clean:
	rm -f tracing.bc *.d

-include *.d
