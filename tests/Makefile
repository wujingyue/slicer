all:
	make -C int

install:
	make -C int install # int-test.cpp

build:
	make -C nocrit
	make -C id-manager
	make -C trace
	make -C trace landmark-trace
	make -C max-slicing
	make -C simplifier

run:
	make -C int run
