##===- lib/Transforms/Hello/Makefile -----------------------*- Makefile -*-===##
# 
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
# 
##===----------------------------------------------------------------------===##

LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj
LIBRARYNAME = test-alias
LOADABLE_MODULE = 1

include $(LEVEL)/Makefile.common

CXXFLAGS += -I$(LLVM_ROOT)

test:: ../aget-nocrit.slice.bc
	opt -disable-output \
		-load $(LLVM_ROOT)/install/lib/libidm.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libtest-alias.so \
		-test-alias \
		< ../aget-nocrit.slice.bc
