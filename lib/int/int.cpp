#include "llvm/PassRegistry.h"
#include "slicer/InitializePasses.h"
using namespace llvm;

struct RegisterIntPasses {
	RegisterIntPasses() {
		PassRegistry &reg = *PassRegistry::getPassRegistry();
		initializeAdvancedAliasPass(reg);
		initializeCaptureConstraintsPass(reg);
		initializeSolveConstraintsPass(reg);
		initializeIteratePass(reg);
		initializeCountCtxtsPass(reg);
		initializeMayWriteAnalyzerPass(reg);
		initializeStratifyLoadsPass(reg);
	}
};
static RegisterIntPasses X;
