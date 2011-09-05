#ifndef __SLICER_CLONE_INFO_MANAGER_H
#define __SLICER_CLONE_INFO_MANAGER_H

#include "llvm/Pass.h"
#include "llvm/Instruction.h"
#include "llvm/ADT/DenseSet.h"
#include "common/typedefs.h"
#include "common/util.h"
#include "common/IDManager.h"
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
		
		bool has_clone_info(const Instruction *ins) const;
		/**
		 * Assertion failure if the instruction does not have any clone info. 
		 * Call <has_clone_info> beforehand. 
		 */
		CloneInfo get_clone_info(const Instruction *ins) const;
		/**
		 * Returns all instructions with this clone_info. 
		 */
		const InstList &get_instructions(
				int thr_id, size_t trunk_id, unsigned orig_ins_id) const;
		/**
		 * Returns any instruction in Thread <thr_id>. 
		 * Returns NULL if not found. 
		 *
		 * TODO: The current implementation scans through the entire <rmap>. 
		 * Pretty slow. Don't use it frequently. 
		 */
		Instruction *get_any_instruction(int thr_id) const;

	private:
		DenseMap<CloneInfo, InstList> rmap;
	};
}

#endif
