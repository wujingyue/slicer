#ifndef __SLICER_CLONE_MAP_MANAGER_H
#define __SLICER_CLONE_MAP_MANAGER_H

#include "llvm/Pass.h"
#include "common/include/typedefs.h"
using namespace llvm;

#include <map>
using namespace std;

namespace slicer {

	struct CloneMapManager: public ModulePass {

		static char ID;

		CloneMapManager(): ModulePass(&ID) {}
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);

		vector<int> get_thr_ids() const;
		size_t get_n_trunks(int thr_id) const;
		Instruction *get_cloned_inst(
				int thr_id, size_t trunk_id, Instruction *old_inst) const;
		Instruction *get_orig_inst(Instruction *new_inst) const;
		int get_thr_id(Instruction *new_inst) const;
		size_t get_trunk_id(Instruction *new_inst) const;

	private:
		// Maps from a cloned instruction to the original instruction. 
		InstMapping clone_map_r;
		// The map between each cloned instruction to the trunk ID. 
		DenseMap<Instruction *, size_t> cloned_to_trunk;
		DenseMap<Instruction *, int> cloned_to_tid;
		// An original instruction can be mapped to multiple instructions in
		// the cloned program. However, there can be at most one of them in each
		// trunk. Therefore, each trunk has a clone map.
		map<int, vector<InstMapping> > clone_map;
	};
}

#endif
