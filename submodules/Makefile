LEVEL = ..
DIRS = rcs

include $(LEVEL)/Makefile.common

all:: stp/build/stp

stp/build/stp:
	mkdir -p stp/build
	cd stp/build; cmake -DCMAKE_BUILD_TYPE=Release ..; make
