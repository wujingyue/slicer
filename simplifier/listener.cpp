#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#include "config.h"
#include "listener.h"
using namespace slicer;

void SimplifierListener::passRegistered(const PassInfo *P) {
#ifdef VERBOSE
	errs() << "Pass " << P->getPassArgument() << " registered\n";
#endif
	if (strcmp(P->getPassArgument(), "capture-constraints") == 0)
		BranchRemover = P;
	if (strcmp(P->getPassArgument(), "iterate") == 0)
		Iterator = P;
}
