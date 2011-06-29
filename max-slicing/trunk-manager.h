#ifndef __SLICER_TRUNK_MANAGER_H
#define __SLICER_TRUNK_MANAGER_H

#include "llvm/Pass.h"
#include "llvm/Instruction.h"
#include "llvm/ADT/DenseSet.h"
#include "common/include/typedefs.h"
using namespace llvm;

#include <vector>
#include <climits>
using namespace std;

namespace slicer {

	struct TrunkManager: public ModulePass {

		static char ID;

		TrunkManager(): ModulePass(&ID) {}
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);
		
		/**
		 * Given an instruction, outputs a list of trunks in which
		 * this instruction may be executed in the sliced program. 
		 */
		void get_containing_trunks(
				const Instruction *ins,
				vector<pair<int, size_t> > &containing_trunks) const;

	private:
		void search_containing_trunks(
				const Instruction *ins,
				ConstInstSet &visited,
				vector<pair<int, size_t> > &containing_trunks) const;
	};
}

#endif
