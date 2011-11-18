#include "llvm/PassRegistry.h"
#include "slicer/InitializePasses.h"
using namespace llvm;

struct RegisterMetricsPasses {
	RegisterMetricsPasses() {
		PassRegistry &reg = *PassRegistry::getPassRegistry();
		initializeBackEdgePass(reg);
		initializeInstCounterPass(reg);
		initializePathCounterPass(reg);
	}
};
