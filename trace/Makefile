##===- lib/Transforms/Hello/Makefile -----------------------*- Makefile -*-===##
# 
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
# 
##===----------------------------------------------------------------------===##

LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj
LIBRARYNAME = slicer-trace
LOADABLE_MODULE = 1

include $(LEVEL)/Makefile.common

CXXFLAGS += -I$(LLVM_ROOT) -I$(SLICER_ROOT) -frtti
LDFLAGS += -lboost_regex

all::
	make -C tracing
	make -C display

clean::
	make -C tracing clean
	make -C display clean
