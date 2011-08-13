/**
 * Author: Jingyue
 *
 * For each instruction in the sliced part, computes its previous
 * enforcing landmark and its next enforcing landmark. 
 *
 * A region is a set of contiguous trunks bounded by enforcing landmarks. 
 *
 * TODO: Maintain a list of regions for each thread. 
 */

#ifndef __SLICER_REGION_MANAGER_H
#define __SLICER_REGION_MANAGER_H

#include "llvm/Pass.h"
#include "common/include/util.h"
using namespace llvm;

#include <vector>
using namespace std;

namespace slicer {

	struct Region {
		Region(int tid, size_t prev, size_t next):
			thr_id(tid), prev_enforcing_landmark(prev),
			next_enforcing_landmark(next) {}
		int thr_id;
		size_t prev_enforcing_landmark;
		size_t next_enforcing_landmark;
	};
	bool operator<(const Region &a, const Region &b);
	bool operator==(const Region &a, const Region &b);
	raw_ostream &operator<<(raw_ostream &O, const Region &r);
}

namespace llvm {

	// So that Region can be used as a DenseMap key. 
	template <> struct DenseMapInfo<slicer::Region> {
		static inline slicer::Region getEmptyKey() {
			return slicer::Region(-1, -1, -1);
		}
		static inline slicer::Region getTombstoneKey() {
			return slicer::Region(-2, -1, -1);
		}
		static unsigned getHashValue(const slicer::Region &r) {
			return DenseMapInfo<pair<int, unsigned> >::getHashValue(make_pair(
						r.thr_id,
						DenseMapInfo<pair<size_t, size_t> >::getHashValue(
							make_pair(
								r.prev_enforcing_landmark,
								r.next_enforcing_landmark))));
		}
		static bool isEqual(const slicer::Region &a, const slicer::Region &b) {
			return a.thr_id == b.thr_id &&
				a.prev_enforcing_landmark == b.prev_enforcing_landmark &&
				a.next_enforcing_landmark == b.next_enforcing_landmark;
		}
	};
}

namespace slicer {

	struct RegionManager: public ModulePass {

		static char ID;
		RegionManager(): ModulePass(&ID) {}
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);
		virtual void print(raw_ostream &O, const Module *M) const;

		void get_containing_regions(
				const Instruction *ins, vector<Region> &regions) const;
		bool region_has_insts(const Region &r) const;
		const ConstInstList &get_insts_in_region(const Region &r) const;
		/**
		 * Returns all regions that are concurrent with a particular region. 
		 * This function does not involve with max-slicer actually.
		 * Should we put it to folder <trace>? 
		 */
		void get_concurrent_regions(
				const Region &r, vector<Region> &regions) const;
		bool concurrent(const Region &a, const Region &b) const;
		bool happens_before(const Region &a, const Region &b) const;
		Region next_region(const Region &r) const;
		Region prev_region(const Region &r) const;
		Region next_region_in_thread(const Region &r, int thr_id) const;

	private:
		void mark_region(
				const InstList &s_insts, const InstList &e_insts,
				int thr_id, size_t s_tr, size_t e_tr);

		void search_containing_regions(const Instruction *ins,
				ConstInstSet &visited, vector<Region> &regions) const;

		// TODO: Using Region * would be faster. 
		DenseMap<const Instruction *, Region> ins_region;
		// The reverse mapping of <ins_region>. 
		// Note that it does not necessarily include all instructions. 
		DenseMap<Region, ConstInstList> region_insts;
	};
}

#endif
