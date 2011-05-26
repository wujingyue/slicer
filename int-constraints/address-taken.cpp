/**
 * Author: Jingyue
 *
 * Collect integer constraints on address-taken variables. 
 */
#include "config.h"
#include "capture.h"
#include "must-alias.h"
#include "common/reach/icfg.h"
#include "idm/mbb.h"
using namespace llvm;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

namespace slicer {

	void CaptureConstraints::add_addr_taken_eq(Value *v1, Value *v2) {
		if (v1 > v2)
			swap(v1, v2);
		addr_taken_eqs.push_back(make_pair(v1, v2));
	}

	void CaptureConstraints::get_all_sources(
			Instruction *ins, Value *p, ValueList &srcs) {
		MicroBasicBlockBuilder &MBB = getAnalysis<MicroBasicBlockBuilder>();
		MicroBasicBlock *mbb = MBB.parent(ins);
		srcs.clear();
		search_all_sources(mbb, ins, p, srcs);
	}

	void CaptureConstraints::search_all_sources(
			MicroBasicBlock *mbb, MicroBasicBlock::iterator ins,
			Value *p0, ValueList &srcs) {
		while (ins != mbb->begin()) {
			--ins;
			Value *p = NULL, *v = NULL;
			if (LoadInst *li = dyn_cast<LoadInst>(ins)) {
				p = li->getPointerOperand();
				v = li;
			} else if (StoreInst *si = dyn_cast<StoreInst>(ins)) {
				p = si->getPointerOperand();
				// getValueOperand
				v = si->getOperand(1);
			}
			if (!p)
				continue;
			BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
			MustAlias &MA = getAnalysis<MustAlias>();
			if (BAA.alias(p, 0, p0, 0))
				srcs.push_back(v);
			if (MA.must_alias(p, p0))
				return;
		}
		assert(false && "Not implemented");
		ICFG &icfg = getAnalysis<ICFG>();
		for (MBBList::iterator it = icfg.parent_begin(mbb), E = icfg.parent_end(mbb);
				it != E; ++it) {
			// TODO: to be implemented
		}
	}

	void CaptureConstraints::capture_addr_taken(Module &M) {
		forallinst(M, ii) {
			if (LoadInst *li = dyn_cast<LoadInst>(ii)) {
				ValueList srcs;
				get_all_sources(li, li->getPointerOperand(), srcs);
				for (size_t i = 0; i < srcs.size(); ++i)
					add_addr_taken_eq(li, srcs[i]);
			}
		}
	}

#if 0
	void CaptureConstraints::print_alias_set(
			raw_ostream &O, const ConstValueSet &as) {
		O << "Must-aliasing set:\n";
		forallconst(ConstValueSet, it, as)
			print_value(O, *it);
	}
#endif
}
