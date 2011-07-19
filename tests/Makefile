all: build

build:
	make -C nocrit
	make -C id-manager
	make -C trace
	make -C trace landmark-trace
	make -C max-slicing
	make -C simplifier
	make -C int

install: build
	make -C int install # int-test.cpp

run:
	make -C int run
