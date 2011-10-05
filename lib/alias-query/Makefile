LEVEL = ../..

LIBRARYNAME = alias-query

LOADABLE_MODULE = 1

CXXFLAGS += -I$(LLVM_ROOT)/install/include -I$(LLVM_ROOT)

include $(LEVEL)/Makefile.common

# Must be put after the include
LIBS += -lboost_regex
