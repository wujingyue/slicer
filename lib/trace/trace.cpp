#include "llvm/PassRegistry.h"
#include "slicer/InitializePasses.h"
using namespace llvm;

struct RegisterTracePasses {
	RegisterTracePasses() {
		PassRegistry &reg = *PassRegistry::getPassRegistry();
		initializeEnforcingLandmarksPass(reg);
		initializeInstrumentPass(reg);
		initializeLandmarkTraceBuilderPass(reg);
		initializeLandmarkTracePass(reg);
		initializeMarkLandmarksPass(reg);
		initializeTraceManagerPass(reg);
		initializeValidityCheckerPass(reg);
	}
};
static RegisterTracePasses X;
