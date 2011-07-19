all:
	make -C stp
	make -C trace
	make -C max-slicing
	make -C int
	make -C reducer
	make -C simplifier

install:
	make -C stp install
	make -C trace install
	make -C max-slicing install
	make -C int install
	make -C reducer install
	make -C simplifier install

clean:
	make -C stp clean
	make -C trace clean
	make -C max-slicing clean
	make -C int clean
	make -C reducer clean
	make -C simplifier clean

dist-clean: clean
	make -C stp dist-clean

.PHONY: clean dist-clean install
