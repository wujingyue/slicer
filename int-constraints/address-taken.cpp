/**
 * Author: Jingyue
 *
 * Collect integer constraints on address-taken variables. 
 */
#include "llvm/Analysis/Dominators.h"
#include "config.h"
#include "common/reach/icfg.h"
#include "common/reach/intra-reach.h"
#include "idm/mbb.h"
using namespace llvm;

#include "capture.h"
#include "exec-once.h"
#include "must-alias.h"
#include "adv-alias.h"

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
	
	const Value *CaptureConstraints::get_pointer_operand(
			const Instruction *i) const {
		if (const StoreInst *si = dyn_cast<StoreInst>(i))
			return si->getPointerOperand();
		if (const LoadInst *li = dyn_cast<LoadInst>(i))
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

	const Value *CaptureConstraints::get_value_operand(
			const Instruction *i) const {
		if (const StoreInst *si = dyn_cast<StoreInst>(i))
			return si->getOperand(0);
		if (const LoadInst *li = dyn_cast<LoadInst>(i))
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
		/*
		 * store v1, p
		 * v2 = load q
		 * p and q may alias
		 * =>
		 * v2 may = v1
		 */
		vector<StoreInst *> all_stores;
		vector<LoadInst *> all_loads;
		forallinst(M, ii) {
			ExecOnce &EO = getAnalysis<ExecOnce>();
			if (EO.not_executed(ii))
				continue;
			Value *v = get_value_operand(ii);
			if (!v)
				continue;
			if (isa<IntegerType>(v->getType()) || isa<PointerType>(v->getType())) {
				if (StoreInst *si = dyn_cast<StoreInst>(ii))
					all_stores.push_back(si);
				if (LoadInst *li = dyn_cast<LoadInst>(ii))
					all_loads.push_back(li);
			}
		}
		errs() << "# of loads = " << all_loads.size() << "\n";
		errs() << "# of stores = " << all_stores.size() << "\n";
		for (size_t i = 0; i < all_loads.size(); ++i) {
#if 0
			errs() << "capture_addr_taken: " << i << "/" << all_loads.size() << "\n";
#endif
			Clause *disj = NULL;
			for (size_t j = 0; j < all_stores.size(); ++j) {
				if (AA->alias(
							all_loads[i]->getPointerOperand(), 0,
							all_stores[j]->getPointerOperand(), 0)) {
					ObjectID &OI = getAnalysis<ObjectID>();
					if (OI.getValueID(all_loads[i]) == 9175) {
						errs() << "source:" << *all_stores[j] << "\n";
					}
					// If at least one of them is not constant, the loaded value
					// can be anything. So, no constraint will be captured in
					// this case. 
					if (!is_constant(all_stores[j]->getOperand(0))) {
						if (disj)
							delete disj;
						disj = NULL;
						break;
					}
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
		// TODO: we currently perform this analysis intra-procedurally
		forallfunc(M, fi)
			capture_overwritten_in_func(fi);
	}

	void CaptureConstraints::capture_overwritten_in_func(Function *fi) {
		forall(Function, bi, *fi) {
			forall(BasicBlock, ii, *bi) {
				if (LoadInst *i2 = dyn_cast<LoadInst>(ii)) {
					Value *q = i2->getPointerOperand();
					// Find the latest dominator <i1> that stores to or loads from
					// a pointer that must alias with <q>. 
					Instruction *i1 = get_idom(i2);
					while (i1) {
						Value *p = get_pointer_operand(i1);
						if (p && AA->alias(p, 0, q, 0) == AliasAnalysis::MustAlias)
							break;
						i1 = get_idom(i1);
					}
					// Is there any store along the path from <i1> to <i2>
					// that may overwrite to <q>?
					if (i1 && !may_write(i1, i2, q)) {
						Clause *c = new Clause(new BoolExpr(
									CmpInst::ICMP_EQ,
									new Expr(i2),
									new Expr(get_value_operand(i1))));
						constraints.push_back(c);
					}
				}
			}
		}
	}

	bool CaptureConstraints::may_write(const Instruction *i, const Value *q) {
		if (const StoreInst *si = dyn_cast<StoreInst>(i)) {
			if (AA->alias(si->getPointerOperand(), 0, q, 0) != AliasAnalysis::NoAlias)
				return true;
		}
		return false;
	}

	bool CaptureConstraints::may_write(
			const Instruction *i1, const Instruction *i2, const Value *q) {
		IntraReach &IR = getAnalysis<IntraReach>(
				*const_cast<Function *>(i1->getParent()->getParent()));
		ConstBBSet visited;
		bool ret = IR.dfs_r(i2->getParent(), i1->getParent(), visited);
		assert(ret && "<i1> should dominate <i2>");
		forall(ConstBBSet, it, visited) {
			const BasicBlock *bb = *it;
			BasicBlock::const_iterator s = bb->begin(), e = bb->end();
			if (i1->getParent() == bb) {
				s = i1;
				++s;
			}
			if (i2->getParent() == bb)
				e = i2;
			for (BasicBlock::const_iterator i = s; i != e; ++i) {
				if (may_write(i, q))
					return true;
			}
		}
		return false;
	}

	BasicBlock *CaptureConstraints::get_idom(BasicBlock *bb) {
		DominatorTree &DT = getAnalysis<DominatorTree>(*bb->getParent());
		DomTreeNode *node = DT[bb];
		DomTreeNode *idom = node->getIDom();
		return (idom ? idom->getBlock() : NULL);
	}

	Instruction *CaptureConstraints::get_idom(Instruction *ins) {
		BasicBlock *bb = ins->getParent();
		if (ins == bb->begin()) {
			BasicBlock *idom_bb = get_idom(bb);
			if (!idom_bb)
				return NULL;
			else
				return idom_bb->getTerminator();
		} else {
			return --(BasicBlock::iterator)ins;
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
