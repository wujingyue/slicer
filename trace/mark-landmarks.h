#ifndef __TERN_MARK_LANDMARKS_H
#define __TERN_MARK_LANDMARKS_H

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "common/include/typedefs.h"

using namespace std;
using namespace llvm;

namespace slicer {
	
	struct MarkLandmarks: public ModulePass {

		static char ID;

		MarkLandmarks(): ModulePass(&ID) {}

		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;
		bool is_landmark(Instruction *ins) const;
		const InstSet &get_landmarks() const;

	private:
		void mark_enforcing_landmarks(Module &M);
		// Mark the successors of each important branch. 
		void mark_branch_succs(Module &M);
		// Mark the entry and the exits of each thread function. 
		// Function main is considered as a thread function as well. 
		void mark_thread(Module &M);
		// Read landmarks from the specified file. 
		void read_landmarks(const string &cut_file);
		// DEPRECATED: Added this because of Tern
		// Get the first non-intrinsic instruction in a basic block. 
		// Returns NULL if all instructions in its BB are intrinsic. 
		Instruction *get_first_non_intrinsic(Instruction *start) const;
		InstSet landmarks;
	};
}

#endif

