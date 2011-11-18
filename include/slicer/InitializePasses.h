/**
 * Author: Jingyue
 */

#ifndef __SLICER_INITIALIZE_PASSES_H
#define __SLICER_INITIALIZE_PASSES_H

namespace llvm {
	class PassRegistry;

	void initializeQueryDriverPass(PassRegistry &);
	void initializeQueryGeneratorPass(PassRegistry &);
	void initializeQueryTranslatorPass(PassRegistry &);

	void initializeAdvancedAliasPass(PassRegistry &);
	void initializeCaptureConstraintsPass(PassRegistry &);
	void initializeSolveConstraintsPass(PassRegistry &);
	void initializeIteratePass(PassRegistry &);
	void initializeCountCtxtsPass(PassRegistry &);
	
	void initializeCloneInfoManagerPass(PassRegistry &);
	void initializeMaxSlicingPass(PassRegistry &);
	void initializeRegionManagerPass(PassRegistry &);

	void initializePreparerPass(PassRegistry &);

	void initializeAggressiveLoopUnrollPass(PassRegistry &);
	void initializeAggressivePromotionPass(PassRegistry &);
	void initializeAssertEqRemoverPass(PassRegistry &);
	void initializeConstantizerPass(PassRegistry &);

	void initializeEnforcingLandmarksPass(PassRegistry &);
	void initializeInstrumentPass(PassRegistry &);
	void initializeLandmarkTraceBuilderPass(PassRegistry &);
	void initializeLandmarkTracePass(PassRegistry &);
	void initializeMarkLandmarksPass(PassRegistry &);
	void initializeTraceManagerPass(PassRegistry &);

	void initializeBackEdgePass(PassRegistry &);
	void initializeInstCounterPass(PassRegistry &);
	void initializePathCounterPass(PassRegistry &);
}

#endif
