all:
	make -C int

install:
	make -C int install # int-test

build:
	make -C progs
	make -C preparer
	make -C id-manager
	make -C trace
	make -C trace full-trace
	make -C trace landmark-trace
	make -C max-slicing
	make -C simplifier

run:
	make -C int run

clean:
	make -C progs clean
	make -C preparer clean
	make -C id-manager clean
	make -C trace clean
	make -C max-slicing clean
	make -C simplifier clean
	make -C int clean
