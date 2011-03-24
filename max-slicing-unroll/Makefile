##===- lib/Transforms/Hello/Makefile -----------------------*- Makefile -*-===##
# 
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
# 
##===----------------------------------------------------------------------===##

LEVEL = ../../llvm/llvm-obj
LIBRARYNAME = tern-max-slicing-unroll
LOADABLE_MODULE = 1
CFLAGS += -I$(LLVM_ROOT)
CXXFLAGS += -I$(LLVM_ROOT)
include $(LEVEL)/Makefile.common

# Build test from scratch each time. 
test::
	make -C tests -B

clean::
	make -C tests clean
