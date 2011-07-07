#ifndef __SLICER_ENFORCING_LANDMARKS_H
#define __SLICER_ENFORCING_LANDMARKS_H

#include "llvm/Pass.h"
#include "common/include/typedefs.h"
using namespace llvm;

namespace slicer {

	struct EnforcingLandmarks: public ModulePass {

		static char ID;

		EnforcingLandmarks(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;

		bool is_enforcing_landmark(Instruction *ins) const {
			return enforcing_landmarks.count(ins);
		}
		const InstSet &get_enforcing_landmarks() const {
			return enforcing_landmarks;
		}

	private:
		InstSet enforcing_landmarks;
	};
}

#endif
