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

test::
	opt -disable-output \
		-load $(LLVM_ROOT)/install/lib/libid-manager.so \
		-load $(LLVM_ROOT)/install/lib/libbc2bdd.so \
		-load $(LLVM_ROOT)/install/lib/libtest-alias.so \
		-test-alias \
		< ../progs/test-dep.simple.bc

.PHONY: test
