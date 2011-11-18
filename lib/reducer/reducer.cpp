#include "llvm/PassRegistry.h"
#include "slicer/InitializePasses.h"
using namespace llvm;

struct RegisterReducerPasses {
	RegisterReducerPasses() {
		PassRegistry &reg = *PassRegistry::getPassRegistry();
		initializeAggressiveLoopUnrollPass(reg);
		initializeAggressivePromotionPass(reg);
		initializeAssertEqRemoverPass(reg);
		initializeConstantizerPass(reg);
	}
};
static RegisterReducerPasses X;
