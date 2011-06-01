/**
 * Author: Jingyue
 *
 * Collect integer constraints on address-taken variables. 
 */
#include "config.h"
#include "capture.h"
#include "exec-once.h"
#include "must-alias.h"
#include "common/reach/icfg.h"
#include "idm/mbb.h"
using namespace llvm;

namespace slicer {

#if 0
	// TODO: not used
	void CaptureConstraints::add_addr_taken_eq(Value *v1, Value *v2) {
		if (v1 > v2)
			swap(v1, v2);
		addr_taken_eqs.push_back(make_pair(v1, v2));
	}

	// TODO: not used
	void CaptureConstraints::get_all_sources(
			Instruction *ins, Value *p, ValueList &srcs) {
		MicroBasicBlockBuilder &MBB = getAnalysis<MicroBasicBlockBuilder>();
		MicroBasicBlock *mbb = MBB.parent(ins);
		srcs.clear();
		search_all_sources(mbb, ins, p, srcs);
	}

	// TODO: not used
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
		assert_not_implemented();
		ICFG &icfg = getAnalysis<ICFG>();
		for (MBBList::iterator it = icfg.parent_begin(mbb), E = icfg.parent_end(mbb);
				it != E; ++it) {
			// TODO: to be implemented
			assert_not_implemented();
		}
	}
#endif

	Value *CaptureConstraints::get_pointer_operand(Instruction *i) const {
		if (StoreInst *si = dyn_cast<StoreInst>(i))
			return si->getPointerOperand();
		if (LoadInst *li = dyn_cast<LoadInst>(i))
			return li->getPointerOperand();
		return NULL;
	}

	Value *CaptureConstraints::get_value_operand(Instruction *i) const {
		if (StoreInst *si = dyn_cast<StoreInst>(i))
			return si->getOperand(0);
		if (LoadInst *li = dyn_cast<LoadInst>(i))
			return li;
		return NULL;
	}

	void CaptureConstraints::capture_addr_taken(Module &M) {
#if 0
		forallinst(M, ii) {
			if (LoadInst *li = dyn_cast<LoadInst>(ii)) {
				ValueList srcs;
				get_all_sources(li, li->getPointerOperand(), srcs);
				for (size_t i = 0; i < srcs.size(); ++i)
					add_addr_taken_eq(li, srcs[i]);
			}
		}
#endif
		vector<StoreInst *> all_stores;
		vector<LoadInst *> all_loads;
		forallinst(M, ii) {
			ExecOnce &EO = getAnalysis<ExecOnce>();
			if (EO.not_executed(ii))
				continue;
			Value *v = get_value_operand(ii);
			if (v &&
					(isa<IntegerType>(v->getType()) || isa<PointerType>(v->getType()))) {
				if (StoreInst *si = dyn_cast<StoreInst>(ii))
					all_stores.push_back(si);
				if (LoadInst *li = dyn_cast<LoadInst>(ii))
					all_loads.push_back(li);
			}
		}
		errs() << "# of loads = " << all_loads.size() << "\n";
		errs() << "# of stores = " << all_stores.size() << "\n";
		for (size_t i = 0; i < all_loads.size(); ++i) {
			Clause *disj = NULL;
			for (size_t j = 0; j < all_stores.size(); ++j) {
				if (AA->alias(
							all_loads[i]->getPointerOperand(), 0,
							all_stores[j]->getPointerOperand(), 0)) {
					Clause *c = new Clause(new BoolExpr(
								CmpInst::ICMP_EQ,
								new Expr(all_loads[i]),
								new Expr(all_stores[j]->getOperand(0))));
					if (!disj)
						disj = c;
					else {
						assert(disj != c);
						disj = new Clause(Instruction::Or, disj, c);
					}
				}
			}
			if (disj)
				constraints.push_back(disj);
		}
	}

	void CaptureConstraints::replace_aa(AliasAnalysis *new_AA) {
		AA = new_AA;
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
