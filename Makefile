.PHONY:alias-pairs int-constraints max-slicing trace

install: all

all: alias-pairs int-constraints max-slicing  trace


alias-pairs:
	cd alias-pairs && make install

int-constraints: stp/install/bin/stp
	cd int-constraints && ./configure && make install

max-slicing:
	cd max-slicing && ./configure && make install

racy-pairs:
	cd racy-pairs && ./configure && make install

trace:
	cd trace && make install

stp/install/bin/stp: stp
	cd stp && ./clean-install.sh --with-g++='g++ -fPIC' --with-gcc='gcc -fPIC' --with-prefix=$PWD/../install

stp:
	svn co https://stp-fast-prover.svn.sourceforge.net/svnroot/stp-fast-prover/trunk/stp stp

clean:
	rm -rf stp
	cd alias-pairs && make clean
	cd int-constraints && make clean
	cd max-slicing && make clean
	cd racy-pairs && make clean
	cd trace && make clean
