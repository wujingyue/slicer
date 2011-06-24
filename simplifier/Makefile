##===- tools/opt/Makefile ----------------------------------*- Makefile -*-===##
#
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
##===----------------------------------------------------------------------===##
LEVEL = $(shell $(LLVM_ROOT)/scripts/level-to-llvm-root)/llvm-obj
TOOLNAME = simplifier

LINK_COMPONENTS := bitreader bitwriter asmparser instrumentation scalaropts ipo

include $(LEVEL)/Makefile.common
