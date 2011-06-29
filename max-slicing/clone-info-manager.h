#ifndef __SLICER_CLONE_INFO_MANAGER_H
#define __SLICER_CLONE_INFO_MANAGER_H

#include "llvm/Pass.h"
#include "llvm/Instruction.h"
#include "llvm/ADT/DenseSet.h"
#include "common/include/typedefs.h"
using namespace llvm;

#include <vector>
#include <climits>
using namespace std;

namespace slicer {

	struct CloneInfo {
		int thr_id;
		size_t trunk_id;
		unsigned orig_ins_id;
	};

	struct CloneInfoManager: public ModulePass {

		static char ID;

		CloneInfoManager(): ModulePass(&ID) {}
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);
		
		/**
		 * Given an instruction, outputs a list of trunks in which
		 * this instruction may be executed in the sliced program. 
		 */
		void get_containing_trunks(
				const Instruction *ins,
				vector<pair<int, size_t> > &containing_trunks) const;
		bool has_clone_info(const Instruction *ins) const;
		/*
		 * Assertion failure if the instruction does not have any clone info. 
		 * Call <has_clone_info> beforehand. 
		 */
		CloneInfo get_clone_info(const Instruction *ins) const;

	private:
		void search_containing_trunks(
				const Instruction *ins,
				ConstInstSet &visited,
				vector<pair<int, size_t> > &containing_trunks) const;
	};
}

#endif
