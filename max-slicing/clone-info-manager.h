#ifndef __SLICER_CLONE_INFO_MANAGER_H
#define __SLICER_CLONE_INFO_MANAGER_H

#include "llvm/Pass.h"
#include "llvm/Instruction.h"
#include "llvm/ADT/DenseSet.h"
#include "common/include/typedefs.h"
#include "common/include/util.h"
#include "common/id-manager/IDManager.h"
#include "../trace/trace-manager.h"
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
}
using namespace slicer;

namespace llvm {

	template <> struct DenseMapInfo<CloneInfo> {

		static CloneInfo getEmptyKey() {
			CloneInfo ci;
			ci.thr_id = 0;
			ci.trunk_id = 0;
			ci.orig_ins_id = IDManager::INVALID_ID;
			return ci;
		}

		static CloneInfo getTombstoneKey() {
			CloneInfo ci = getEmptyKey();
			ci.thr_id = TraceManager::INVALID_TID;
			return ci;
		}

		static unsigned getHashValue(const CloneInfo &x) {
			return DenseMapInfo<pair<int, pair<size_t, unsigned> > >::getHashValue(
					make_pair(x.thr_id, make_pair(x.trunk_id, x.orig_ins_id)));
		}

		static bool isEqual(const CloneInfo &x, const CloneInfo &y) {
			return x.thr_id == y.thr_id && x.trunk_id == y.trunk_id &&
				x.orig_ins_id == y.orig_ins_id;
		}
	};
}

namespace slicer {

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
		/*
		 * Returns NULL if cannot find such clone_info. 
		 */
		Instruction *get_instruction(
				int thr_id, size_t trunk_id, unsigned orig_ins_id) const;

	private:
		/*
		 * FIXME: Use this function with cautions. It assumes clone_info is
		 * not optimized out.
		 *
		 * e.g. 
		 * inst1; !clone_info 1
		 * inst2; opt'ed, no clone_info
		 * inst3; !clone_info 3
		 *
		 * In this case, inst2 is in the same trunk as either inst1 or inst3. 
		 * But the current implementation will fail on inst2.
		 */
		void search_containing_trunks(
				const Instruction *ins,
				ConstInstSet &visited,
				vector<pair<int, size_t> > &containing_trunks) const;

		DenseMap<CloneInfo, Instruction *> rmap;
	};
}

#endif
