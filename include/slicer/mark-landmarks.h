#ifndef __SLICER_MARK_LANDMARKS_H
#define __SLICER_MARK_LANDMARKS_H

#include "llvm/Pass.h"
#include "llvm/Module.h"
using namespace llvm;

#include "common/typedefs.h"
using namespace rcs;

namespace slicer {
	struct MarkLandmarks: public ModulePass {
		static char ID;

		MarkLandmarks();
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
		/**
		 * Mark enforcing landmarks as landmarks. 
		 */
		void mark_enforcing_landmarks(Module &M);
		/**
		 * Mark exits of thread functions as derived landmarks. 
		 * The preparer has already inserted enforcing landmarks at
		 * thread function exits. So marking function exits as derived landmarks
		 * is safe. 
		 */
		void mark_thread_exits(Module &M);
		/**
		 * Mark all return sites of recursive functions that may execute
		 * enforcing landmarks. 
		 */
		void mark_enforcing_recursive_returns(Module &M);
		/**
		 * Mark all calls to functions that may execute enforcing landmarks. 
		 */
		void mark_enforcing_calls(Module &M);
		/**
		 * Mark the return sits of function <f> as derived landmarks. 
		 */
		void mark_returns(Function *f);

		InstSet landmarks;
	};
}

#endif
