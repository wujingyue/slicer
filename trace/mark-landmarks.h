#ifndef __SLICER_MARK_LANDMARKS_H
#define __SLICER_MARK_LANDMARKS_H

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "common/include/typedefs.h"
using namespace llvm;

namespace slicer {
	
	struct MarkLandmarks: public ModulePass {

		static char ID;

		MarkLandmarks(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

		/**
		 * Enforcing landmarks or derived landmarks.
		 */
		bool is_landmark(Instruction *ins) const { return landmarks.count(ins); }
		/**
		 * Get all enforcing landmarks and derived landmarks.
		 */
		const InstSet &get_landmarks() const;

	private:
		void mark_enforcing_landmarks(Module &M);
		// Mark the successors of each important branch. 
		void mark_branch_succs(Module &M);
		// Mark the entry and the exits of each thread function. 
		// Function main is considered as a thread function as well. 
		void mark_thread(Module &M);

		InstSet landmarks;
	};
}

#endif
