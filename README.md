Schedule Specialization Framework
=================================

Schedule specialization framework is a research prototype we created at
Columbia that specialize a multithreaded program towards a schedule for better
analysis. We implemented this framework based on the LLVM compiler. Given the
bitcode of a program and a schedule in the format of a total order of
synchronizations, the framework specializes the program with respect to the
schedule and emits the bitcode of the specialized program for more precise
analysis.

The entire schedule specialization system contains several components: the
Peregrine determinisitc multithreading system, a precise alias analysis based
on bddbddb (bc2bdd), and the specialization framework.  This repository
contains the source code that implements the specialization framework. The
source code of the Peregrine system is available
[here](https://github.com/columbia/xtern), and we are in the process of
open-sourcing bc2bdd.

Publications
------------

[Sound and Precise Analysis of Parallel Programs through Schedule
Specialization](http://www.cs.columbia.edu/~junfeng/papers/wu-pldi12.pdf)

Installation
------------

1. Download the source code of LLVM 3.1 and clang 3.1 from
   [LLVM Download Page](http://llvm.org/releases/download.html). Other version
of LLVM and clang are not guaranteed to work with NeonGoby.

2. Build LLVM and clang from source code.
```bash
cd <llvm-source-code-root>
mv <clang-source-code-root> tools/clang
./configure --prefix=<where-you-want-to-install-LLVM>
make [-j] install
```

3. Add LLVM's install directory to PATH, so that you can run LLVM commands
   (e.g., `llvm-config`) everywhere.

4. To verify whether LLVM/clang 3.1 is correctly, compile and run a simple
   hello world program. Clang 3.1 does not work with some new versions of
   Ubuntu on some programs using STL, because the default include paths Clang
   sets up are incomplete. In that case, you may want to apply
   `patches/clang-3.1-ubuntu.patch`.
```bash
cd <llvm-source-code-root>
patch -p1 < patches/clang-3.1-ubuntu.patch
```

5. Build the specialization framework (a.k.a. slicer)
```bash
git clone https://github.com/wujingyue/slicer.git
cd slicer
git submodule init
git submodule update
./configure --prefix=`llvm-config --prefix`
make
make install
```
