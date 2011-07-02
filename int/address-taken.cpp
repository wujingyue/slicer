/**
 * Author: Jingyue
 *
 * Collect integer constraints on address-taken variables. 
 */
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/DominatorInternals.h"
#include "llvm/Support/Timer.h"
#include "config.h"
#include "idm/mbb.h"
#include "common/callgraph-fp/callgraph-fp.h"
#include "common/cfg/icfg.h"
#include "common/cfg/intra-reach.h"
#include "common/cfg/exec-once.h"
#include "common/cfg/partial-icfg-builder.h"
#include "common/cfg/reach.h"
#include "../max-slicing/clone-info-manager.h"
#include "../trace/trace-manager.h"
using namespace llvm;

#include "capture.h"
#include "must-alias.h"
#include "adv-alias.h"
#include "../trace/landmark-trace.h"
using namespace slicer;

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

	/*
	 * store v1, p (including global variable initializers)
	 * v2 = load q
	 * p and q may alias
	 * =>
	 * v2 may = v1
	 */
	// <value, pointer>
	vector<pair<const Value *, const Value *> > all_stores, all_loads;
	// Scan through all loads and stores. 
	// NOTE: We don't guarantee these stores and loads are constant. 
	// Actually, we need consider non-constant stores here, because any
	// instruction that loads from it may end up loading anything. 
	forallinst(M, ii) {
		ExecOnce &EO = getAnalysis<ExecOnce>();
		if (EO.not_executed(ii))
			continue;
		Value *v = get_value_operand(ii);
		if (!v)
			continue;
		if (isa<IntegerType>(v->getType()) || isa<PointerType>(v->getType())) {
			if (StoreInst *si = dyn_cast<StoreInst>(ii))
				all_stores.push_back(make_pair(v, si->getPointerOperand()));
			if (LoadInst *li = dyn_cast<LoadInst>(ii))
				all_loads.push_back(make_pair(v, li->getPointerOperand()));
		}
	}
	// Scan through all global variables. 
	for (Module::global_iterator gi = M.global_begin();
			gi != M.global_end(); ++gi) {
		// FIXME: We ignore pointers here. They are usually initialized as NULL.
		// <gi> itself must be a pointer. 
		assert(isa<PointerType>(gi->getType()));
		const Type *ele_type =
			dyn_cast<PointerType>(gi->getType())->getElementType();
		if (!isa<IntegerType>(ele_type))
			continue;
		if (gi->hasInitializer()) {
			if (ConstantInt *ci = dyn_cast<ConstantInt>(gi->getInitializer()))
				all_stores.push_back(make_pair(ci, gi));
		}
	}
	errs() << "# of loads = " << all_loads.size() << "\n";
	errs() << "# of stores = " << all_stores.size() << "\n";

	Timer tmr_pairwise("Pairwise");
	tmr_pairwise.startTimer();
	for (size_t i = 0; i < all_loads.size(); ++i) {
		if (!is_constant(all_loads[i].first))
			continue;
		Clause *disj = NULL;
		for (size_t j = 0; j < all_stores.size(); ++j) {
			if (AA->alias(all_loads[i].second, 0, all_stores[j].second, 0)) {
				// If the stored value is not constant, the loaded value
				// can be anything. So, no constraint will be captured in
				// this case. 
				if (!is_constant(all_stores[j].first)) {
					// errs() << "[Warning] Stores a variable\n";
					if (disj)
						delete disj;
					disj = NULL;
					break;
				}
				Clause *c = new Clause(new BoolExpr(
							CmpInst::ICMP_EQ,
							new Expr(all_loads[i].first),
							new Expr(all_stores[j].first)));
				if (!disj)
					disj = c;
				else {
					assert(disj != c);
					disj = new Clause(Instruction::Or, disj, c);
				}
			}
		} // for store
		if (disj)
			constraints.push_back(disj);
	}
	tmr_pairwise.stopTimer();

	// TODO: we currently perform this analysis intra-procedurally
	Timer tmr_overwritten("Overwritten");
	tmr_overwritten.startTimer();
	forallfunc(M, fi) {
		if (fi->isDeclaration())
			continue;
		capture_overwritten_in_func(fi);
	}
	tmr_overwritten.stopTimer();
}

Instruction *CaptureConstraints::find_latest_overwriter(
		Instruction *i2, Value *q) {
	PartialICFGBuilder &PIB = getAnalysis<PartialICFGBuilder>();
	MicroBasicBlockBuilder &MBBB = getAnalysis<MicroBasicBlockBuilder>();
	MicroBasicBlock *m2 = MBBB.parent(i2);
	if (!PIB[m2])
		return NULL;
	// Find the latest dominator <i1> that stores to or loads from
	// a pointer that must alias with <q>. 
	Instruction *i1 = get_idom_ip(i2);
	while (i1) {
		Value *p = get_pointer_operand(i1);
		if (p) {
			AliasAnalysis::AliasResult res = AA->alias(p, 0, q, 0);
			if (res == AliasAnalysis::MustAlias)
				break;
		}
		i1 = get_idom_ip(i1);
	}
	return i1;
}

void CaptureConstraints::capture_overwriting_to(LoadInst *i2) {
	
	if (!is_constant(i2))
		return;
	
	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
	vector<pair<int, size_t> > cur_trunks;
	CIM.get_containing_trunks(i2, cur_trunks);
	if (cur_trunks.size() != 1)
		return;
	int cur_thr_id = cur_trunks[0].first;
	size_t cur_trunk_id = cur_trunks[0].second;

	errs() << cur_thr_id << ' ' << cur_trunk_id << ":" << *i2 << "\n";

	TraceManager &TM = getAnalysis<TraceManager>();
	LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
	vector<int> thr_ids = LT.get_thr_ids();
	vector<Instruction *> latest_doms(thr_ids.size(), NULL);
	for (size_t k = 0; k < thr_ids.size(); ++k) {
		int i = thr_ids[k];
		if (i == cur_thr_id)
			latest_doms[k] = i2;
		else {
			size_t j = LT.get_latest_happens_before(cur_thr_id, cur_trunk_id, i);
			if (j != (size_t)-1) {
				// TraceManager is still looking at the trace for the original program.
				// But, we should use the cloned instruction. 
				unsigned orig_ins_id = TM.get_record(LT.get_landmark(i, j)).ins_id;
				latest_doms[k] = CIM.get_instruction(i, j, orig_ins_id);
				assert(latest_doms[k] && "Cannot find the cloned landmark");
			}
		}
	}

	Value *q = i2->getPointerOperand();
	vector<Instruction *> latest_overwriters(thr_ids.size(), NULL);
	for (size_t k = 0; k < thr_ids.size(); ++k) {
		if (latest_doms[k])
			latest_overwriters[k] = find_latest_overwriter(latest_doms[k], q);
	}

	vector<pair<size_t, size_t> > overwriter_trunks;
	overwriter_trunks.resize(thr_ids.size());
	for (size_t k = 0; k < thr_ids.size(); ++k) {
		Instruction *i1 = latest_overwriters[k];
		if (!i1) {
			overwriter_trunks[k] = make_pair((size_t)-1, 0);
			continue;
		}
		vector<pair<int, size_t> > containing_trunks;
		CIM.get_containing_trunks(i1, containing_trunks);
		size_t s = (size_t)-1, e = 0;
		for (size_t t = 0; t < containing_trunks.size(); ++t) {
			if (containing_trunks[t].first == thr_ids[k]) {
				s = min(s, containing_trunks[t].second);
				e = max(e, containing_trunks[t].second);
			}
		}
		LT.extend_until_enforce(thr_ids[k], s, e);
		overwriter_trunks[k] = make_pair(s, e);
	}

	for (size_t k1 = 0; k1 < thr_ids.size(); ++k1) {
		if (!latest_overwriters[k1])
			continue;
		bool before = false;
		for (size_t k2 = k1 + 1; k2 < thr_ids.size(); ++k2) {
			if (!latest_overwriters[k2])
				continue;
			// If latest_overwriters[k1] must happen before latest_overwriters[k2], 
			// remove latest_overwriters[k1]. 
			if (LT.happens_before(
						thr_ids[k1], overwriter_trunks[k1].second,
						thr_ids[k2], overwriter_trunks[k2].first)) {
				before = true;
				break;
			}
		}
		if (before)
			latest_overwriters[k1] = NULL;
	}

	unsigned n_overwriters = 0;
	int the_thr_idx = -1; // Used later. 
	for (size_t k = 0; k < thr_ids.size(); ++k) {
		if (latest_overwriters[k]) {
			n_overwriters++;
			the_thr_idx = k;
		}
	}

	if (n_overwriters == 0) {
		if (GlobalVariable *gv = dyn_cast<GlobalVariable>(q)) {
			if (gv->hasInitializer()) {
				if (ConstantInt *ci = dyn_cast<ConstantInt>(gv->getInitializer())) {
					Clause *c = new Clause(new BoolExpr(
								CmpInst::ICMP_EQ,
								new Expr(i2),
								new Expr(ci)));
					constraints.push_back(c);
				}
			}
		}
	} else if (n_overwriters == 1) {
		if (!path_may_write(
					latest_overwriters[the_thr_idx], latest_doms[the_thr_idx], q)) {
			Clause *c = new Clause(new BoolExpr(
						CmpInst::ICMP_EQ,
						new Expr(i2),
						new Expr(get_value_operand(latest_overwriters[the_thr_idx]))));
			constraints.push_back(c);
		}
	}
}

void CaptureConstraints::capture_overwritten_in_func(Function *fi) {
	ExecOnce &EO = getAnalysis<ExecOnce>();
	if (EO.not_executed(fi))
		return;
	forall(Function, bi, *fi) {
		forall(BasicBlock, ii, *bi) {
			if (LoadInst *i2 = dyn_cast<LoadInst>(ii))
				capture_overwriting_to(i2);
		}
	}
}

bool CaptureConstraints::may_write(
		const Instruction *i, const Value *q, ConstFuncSet &visited_funcs) {
	if (const StoreInst *si = dyn_cast<StoreInst>(i)) {
		if (AA->alias(si->getPointerOperand(), 0, q, 0) != AliasAnalysis::NoAlias)
			return true;
	}
	// If <i> is a function call, go into the function. 
	if (is_call(i)) {
		CallGraphFP &CG = getAnalysis<CallGraphFP>();
		FuncList callees = CG.get_called_functions(i);
		for (size_t j = 0; j < callees.size(); ++j) {
			if (may_write(callees[j], q, visited_funcs))
				return true;
		}
	}
	return false;
}

bool CaptureConstraints::may_write(
		const Function *f, const Value *q, ConstFuncSet &visited_funcs) {
	if (visited_funcs.count(f))
		return false;
	visited_funcs.insert(f);
	// FIXME: need function summary
	// For now, we assume external functions don't write to <q>. 
	if (f->isDeclaration())
		return false;
	forallconst(Function, bi, *f) {
		forallconst(BasicBlock, ii, *bi) {
			if (may_write(ii, q, visited_funcs))
				return true;
		}
	}
	return false;
}

bool CaptureConstraints::path_may_write(
		const Instruction *i1, const Instruction *i2, const Value *q) {
#if 0
	errs() << "path_may_write:\n";
	errs() << "\t" << *i1 << "\n" << "\t" << *i2 << "\n";
#endif
	MicroBasicBlockBuilder &MBBB = getAnalysis<MicroBasicBlockBuilder>();
	MicroBasicBlock *m1 = MBBB.parent(i1), *m2 = MBBB.parent(i2);

	ICFG &PIB = getAnalysis<PartialICFGBuilder>();
	ICFGNode *n1 = PIB[m1], *n2 = PIB[m2];
	assert(n1 && n2);
	
	Reach<ICFGNode> IR;
	DenseSet<const ICFGNode *> visited, sink;
	sink.insert(n1);
	IR.floodfill_r(n2, sink, visited);
	assert(visited.count(n1) && "<i1> should dominate <i2>");
	// Functions visited in <may_write>s. 
	// In order to handle recursive functions. 
	// FIXME: Trace into functions that don't appear in the ICFG. 
	forall(DenseSet<const ICFGNode *>, it, visited) {
		const MicroBasicBlock *mbb = (*it)->getMBB();
		BasicBlock::const_iterator s = mbb->begin(), e = mbb->end();
		if (m1 == mbb) {
			s = i1;
			++s;
		}
		if (m2 == mbb)
			e = i2;
		for (BasicBlock::const_iterator i = s; i != e; ++i) {
			if (const StoreInst *si = dyn_cast<StoreInst>(i)) {
				if (AA->alias(si->getPointerOperand(), 0, q, 0) !=
						AliasAnalysis::NoAlias)
					return true;
			}
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

MicroBasicBlock *CaptureConstraints::get_idom_ip(MicroBasicBlock *mbb) {
#if 1
	errs() << "get_idom_ip:\n";
	errs() << mbb->front() << "\n";
	errs() << mbb->back() << "\n";
#endif
	ICFG &PIB = getAnalysis<PartialICFGBuilder>();
	ICFGNode *node = PIB[mbb];
	assert(node);
	DomTreeNodeBase<ICFGNode> *dt_node = IDT.getNode(node);
	assert(dt_node);
	DomTreeNodeBase<ICFGNode> *dt_idom = dt_node->getIDom();
	if (dt_idom)
		assert(dt_idom->getBlock());
	return (dt_idom ? dt_idom->getBlock()->getMBB() : NULL);
}

Instruction *CaptureConstraints::get_idom(Instruction *ins) {
	BasicBlock *bb = ins->getParent();
	if (ins == bb->begin()) {
		BasicBlock *idom_bb = get_idom(bb);
		return (!idom_bb ? NULL : idom_bb->getTerminator());
	} else {
		return --(BasicBlock::iterator)ins;
	}
}

Instruction *CaptureConstraints::get_idom_ip(Instruction *ins) {
	assert(ins);
	errs() << "get_idom_ip:" << *ins << "\n";
	MicroBasicBlockBuilder &MBBB = getAnalysis<MicroBasicBlockBuilder>();
	MicroBasicBlock *mbb = MBBB.parent(ins);
	assert(mbb);
	if (ins == mbb->begin()) {
		MicroBasicBlock *idom_mbb = get_idom_ip(mbb);
		return (idom_mbb ? &idom_mbb->back() : NULL);
	} else {
		return --(BasicBlock::iterator)ins;
	}
}

void CaptureConstraints::replace_aa(AliasAnalysis *new_AA) {
	AA = new_AA;
}
