Schedule Specialization Framework
=================================

Schedule specialization framework is a research prototype we created at
Columbia that specialize a multithreaded program towards a schedule for better
analysis. It is implemented on the LLVM framework.

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

4. Build the specialization framework (a.k.a. slicer)
```bash
git clone https://github.com/wujingyue/slicer.git
cd slicer
git submodule init
git submodule update
./configure --prefix=`llvm-config --prefix`
make
make install
```
