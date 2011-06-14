##===- lib/Transforms/Hello/Makefile -----------------------*- Makefile -*-===##
# 
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
# 
##===----------------------------------------------------------------------===##

LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj
LIBRARYNAME = max-slicing
LOADABLE_MODULE = 1
CXXFLAGS += -I$(LLVM_ROOT) -I$(SLICER_ROOT)
include $(LEVEL)/Makefile.common

# Build test from scratch each time. 
test::
	make -C tests -B

clean::
	make -C tests clean
