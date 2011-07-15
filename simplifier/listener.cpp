#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

#include "config.h"
#include "listener.h"
using namespace slicer;

void SimplifierListener::passRegistered(const PassInfo *P) {
	DEBUG(dbgs() << "Pass " << P->getPassArgument() << " registered\n";);
	if (strcmp(P->getPassArgument(), "pre-reduce") == 0)
		PreReducer = P;
	if (strcmp(P->getPassArgument(), "reduce") == 0)
		Reducer = P;
	if (strcmp(P->getPassArgument(), "iterate") == 0)
		Iterator = P;
	if (strcmp(P->getPassArgument(), "aggressive-loop-unroll") == 0)
		AggressiveLoopUnroll = P;
}
