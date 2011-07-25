##===- lib/Transforms/Hello/Makefile -----------------------*- Makefile -*-===##
# 
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
# 
##===----------------------------------------------------------------------===##

LEVEL = ../../llvm/llvm-obj
LIBRARYNAME = int
LOADABLE_MODULE = 1
BUILD_ARCHIVE = 1
CXXFLAGS += -I$(LLVM_ROOT) -I$(SLICER_ROOT) -I$(SLICER_ROOT)/stp/install/include
LDFLAGS += -L$(SLICER_ROOT)/stp/install/lib

include $(LEVEL)/Makefile.common
# Must be put after the include. 
LIBS += -lstp
