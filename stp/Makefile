all: install/bin/stp

install/bin/stp: stp-svn
	cd stp-svn && ./clean-install.sh --with-g++='g++ -fPIC' --with-gcc='gcc -fPIC' --with-prefix=$(PWD)/install

stp-svn:
	svn co https://stp-fast-prover.svn.sourceforge.net/svnroot/stp-fast-prover/trunk/stp stp-svn
	
test:
	cd stp-svn && echo $(PWD)

clean:
	make -C stp-svn clean

dist-clean: clean
	rm -rf install
