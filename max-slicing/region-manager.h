/**
 * Author: Jingyue
 *
 * For each instruction in the sliced part, computes its previous
 * enforcing landmark and its next enforcing landmark. 
 *
 * A region is a set of contiguous trunks bounded by enforcing landmarks. 
 */

#ifndef __SLICER_REGION_MANAGER_H
#define __SLICER_REGION_MANAGER_H

#include "llvm/Pass.h"
using namespace llvm;

#include <vector>
using namespace std;

namespace slicer {

	struct Region {
		int thr_id;
		size_t prev_enforcing_landmark;
		size_t next_enforcing_landmark;
	};
	bool operator<(const Region &a, const Region &b) {
		return a.thr_id < b.thr_id || (a.thr_id == b.thr_id &&
				b.next_enforcing_landmark < b.next_enforcing_landmark);
	}
	bool operator==(const Region &a, const Region &b) {
		return a.thr_id == b.thr_id &&
			a.prev_enforcing_landmark == b.prev_enforcing_landmark;
	}

	struct RegionManager: public ModulePass {

		static char ID;
		RegionManager(): ModulePass(&ID) {}
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);
		virtual void print(raw_ostream &O, const Module *M) const;

		void get_containing_regions(
				const Instruction *ins, vector<Region> &regions) const;

	private:
		void mark_region(
				const Instruction *s_ins, const Instruction *e_ins,
				int thr_id, size_t s_tr, size_t e_tr);

		void search_containing_regions(const Instruction *ins,
				ConstInstSet &visited, vector<Region> &regions) const;

		DenseMap<const Instruction *, Region> ins_region;
	};
}

#endif
