#ifndef __SLICER_MARK_LANDMARKS_H
#define __SLICER_MARK_LANDMARKS_H

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "common/typedefs.h"
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
		/**
		 * Mark exits of thread functions as derived landmarks. 
		 * The preparer has already inserted enforcing landmarks at
		 * thread function exits. So marking function exits as derived landmarks
		 * is safe. 
		 */
		void mark_thread_exits(Module &M);
		/**
		 * Mark the return sites of recursive functions that may execute
		 * enforcing landmarks. 
		 */
		void mark_recursive_rets(Module &M);
		void mark_recursive_entries(Module &M);
		/**
		 * Mark the entries and exits of all functions that
		 * may execute enforcing landmarks.
		 */
		void mark_enforcing_functions(Module &M);
		/**
		 * Used by mark_recursive_rets.
		 * Mark the return sits of function <f> as derived landmarks. 
		 */
		void mark_rets(Function *f);

		InstSet landmarks;
	};
}

#endif
