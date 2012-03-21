#ifndef __SLICER_ENFORCING_LANDMARKS_H
#define __SLICER_ENFORCING_LANDMARKS_H

#include "llvm/Pass.h"
#include "common/typedefs.h"
using namespace llvm;

namespace slicer {
	struct EnforcingLandmarks: public ModulePass {
		static char ID;

		EnforcingLandmarks();
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;

		bool is_enforcing_landmark(const Instruction *ins) const;
		bool is_blocking_enforcing_landmark(const Instruction *ins) const;
		const InstSet &get_enforcing_landmarks() const;

	private:
		void insert_enforcing_landmark_func(const string &func_name,
				const string &is_blocking);
		void filter_enforcing_landmarks();

		DenseSet<string> enforcing_landmark_funcs;
		DenseSet<string> blocking_enforcing_landmark_funcs;
		InstSet enforcing_landmarks;
	};
}

#endif
