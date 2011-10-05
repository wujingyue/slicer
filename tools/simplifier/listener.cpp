/**
 * Author: Jingyue
 */

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

#include "listener.h"
using namespace slicer;

void SimplifierListener::passRegistered(const PassInfo *P) {
	DEBUG(dbgs() << "Pass " << P->getPassArgument() << " registered\n";);
	NameToPassInfo[P->getPassArgument()] = P;
}

const PassInfo *SimplifierListener::getPassInfo(const string &Name) const {
	map<string, const PassInfo *>::const_iterator I = NameToPassInfo.find(Name);
	return (I == NameToPassInfo.end() ? NULL : I->second);
}
