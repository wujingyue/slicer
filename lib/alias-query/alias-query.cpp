#include "llvm/PassRegistry.h"
#include "slicer/InitializePasses.h"
using namespace llvm;

struct RegisterAliasQueryPasses {
	RegisterAliasQueryPasses() {
		PassRegistry &reg = *PassRegistry::getPassRegistry();
		initializeQueryDriverPass(reg);
		initializeQueryGeneratorPass(reg);
		initializeQueryTranslatorPass(reg);
	}
};
static RegisterAliasQueryPasses X;
