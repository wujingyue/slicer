/**
 * Author: Jingyue
 *
 * Collect integer constraints on address-taken variables. 
 */

#define DEBUG_TYPE "int"

#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/DominatorInternals.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/Debug.h"
#include "config.h"
#include "idm/mbb.h"
#include "common/callgraph-fp/callgraph-fp.h"
#include "common/cfg/icfg.h"
#include "common/cfg/intra-reach.h"
#include "common/cfg/exec-once.h"
#include "common/cfg/partial-icfg-builder.h"
#include "common/cfg/reach.h"
#include "max-slicing/clone-info-manager.h"
#include "max-slicing/region-manager.h"
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

	Timer tmr_pairwise("may-assign");
	tmr_pairwise.startTimer();
	capture_may_assign(M);
	tmr_pairwise.stopTimer();
	
	Timer tmr_overwritten("must-assign");
	tmr_overwritten.startTimer();
	capture_must_assign(M);
	tmr_overwritten.stopTimer();
}

void CaptureConstraints::capture_may_assign(Module &M) {
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
	errs() << "# of loads = " << all_loads.size() << "; ";
	errs() << "# of stores = " << all_stores.size() << "\n";

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
}

void CaptureConstraints::capture_must_assign(Module &M) {

	forallfunc(M, fi) {
		if (fi->isDeclaration())
			continue;
		ExecOnce &EO = getAnalysis<ExecOnce>();
		if (EO.not_executed(fi))
			continue;
		forall(Function, bi, *fi) {
			forall(BasicBlock, ii, *bi) {
				if (LoadInst *i2 = dyn_cast<LoadInst>(ii))
					capture_overwriting_to(i2);
			}
		}
	}
}

Instruction *CaptureConstraints::find_nearest_common_dom(
		Instruction *i1, Instruction *i2) {

	if (i1 == NULL)
		return i2;
	if (i2 == NULL)
		return i1;
	
	MicroBasicBlockBuilder &MBBB = getAnalysis<MicroBasicBlockBuilder>();
	MicroBasicBlock *m1 = MBBB.parent(i1), *m2 = MBBB.parent(i2);
	assert(m1 != m2 && "Not supported currently");
	assert(m1 && m2);

	ICFG &PIB = getAnalysis<PartialICFGBuilder>();
	ICFGNode *n1 = PIB[m1], *n2 = PIB[m2];
	assert(n1 && n2);

	ICFGNode *n = IDT.findNearestCommonDominator(n1, n2);
	assert(n);
	MicroBasicBlock *m = n->getMBB();
	assert(m);
	return &m->back();
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
	
	LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
	RegionManager &RM = getAnalysis<RegionManager>();

	vector<Region> cur_regions;
	RM.get_containing_regions(i2, cur_regions);
	if (cur_regions.size() != 1)
		return;
	int cur_thr_id = cur_regions[0].thr_id;
	size_t prev_enforcing = cur_regions[0].prev_enforcing_landmark;

	DEBUG(dbgs() << "capture_overwriting_to: " << cur_thr_id << ' ' <<
		prev_enforcing << ":" << *i2 << "\n";);

	// Compute the latest dominator in each thread. 
	vector<int> thr_ids = LT.get_thr_ids();
	vector<Instruction *> latest_doms(thr_ids.size(), NULL);
	for (size_t k = 0; k < thr_ids.size(); ++k) {
		
		int i = thr_ids[k];
		if (i == cur_thr_id) {
			latest_doms[k] = i2;
			continue;
		}
		
		if (prev_enforcing == (size_t)-1)
			continue;
		size_t j = LT.get_latest_happens_before(cur_thr_id, prev_enforcing, i);
		if (j == (size_t)-1)
			continue;
		
		// TraceManager is still looking at the trace for the original program.
		// But, we should use the cloned instruction.
		unsigned orig_ins_id = LT.get_landmark(i, j).ins_id;
		const InstList &landmarks = CIM.get_instructions(i, j, orig_ins_id);
		assert(landmarks.size() > 0);
		latest_doms[k] = NULL;
		for (size_t t = 0; t < landmarks.size(); ++t)
			latest_doms[k] = find_nearest_common_dom(latest_doms[k], landmarks[t]);
		if (!latest_doms[k]) {
			errs() << "get_landmark: " << i << ' ' << j <<
				' ' << orig_ins_id << "\n";
		}
		assert(latest_doms[k] && "Cannot find the cloned landmark");
	}

	// Compute the latest overwriter of the latest dominator in each thread. 
	Value *q = i2->getPointerOperand();
	vector<Instruction *> latest_overwriters(thr_ids.size(), NULL);
	for (size_t k = 0; k < thr_ids.size(); ++k) {
		if (latest_doms[k])
			latest_overwriters[k] = find_latest_overwriter(latest_doms[k], q);
	}

	// These latest overwriters may not all be the valid sources of
	// this LoadInst. Compare their containing regions, and prune out
	// those which cannot. 
	vector<pair<size_t, size_t> > overwriter_regions;
	overwriter_regions.resize(thr_ids.size());
	for (size_t k = 0; k < thr_ids.size(); ++k) {
		Instruction *i1 = latest_overwriters[k];
		if (!i1) {
			// An invalid trunk range. 
			overwriter_regions[k] = make_pair((size_t)-1, (size_t)-1);
			continue;
		}
		vector<Region> regions;
		RM.get_containing_regions(i1, regions);
		assert(regions.size() > 0);
		size_t s = (size_t)-1, e = (size_t)-1;
		for (size_t t = 0; t < regions.size(); ++t) {
			if (regions[t].thr_id == thr_ids[k]) {
				if (s == (size_t)-1 || regions[t].prev_enforcing_landmark < s)
					s = regions[t].prev_enforcing_landmark;
				if (e == (size_t)-1 || regions[t].next_enforcing_landmark > e)
					e = regions[t].next_enforcing_landmark;
			}
		}
		overwriter_regions[k] = make_pair(s, e);
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
			size_t t1 = overwriter_regions[k1].second;
			size_t t2 = overwriter_regions[k2].first;
			if (t1 == (size_t)-1 || t2 == (size_t)-1)
				continue;
			assert(LT.is_enforcing_landmark(thr_ids[k1], t1));
			assert(LT.is_enforcing_landmark(thr_ids[k2], t2));
			if (LT.get_landmark_timestamp(thr_ids[k1], t1) <
					LT.get_landmark_timestamp(thr_ids[k2], t2)) {
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

	DEBUG(dbgs() << "# of overwriters = " << n_overwriters << "\n";);
	if (n_overwriters == 0) {
		// TODO: Check whether the value may be overwritten by any trunk from
		// the program start to the current trunk. 
#if 0
		if (GlobalVariable *gv = dyn_cast<GlobalVariable>(q)) {
			if (gv->hasInitializer()) {
				if (ConstantInt *ci = dyn_cast<ConstantInt>(gv->getInitializer())) {
					Clause *c = new Clause(new BoolExpr(
								CmpInst::ICMP_EQ,
								new Expr(i2),
								new Expr(ci)));
					errs() << "From overwriting: ";
					print_clause(errs(), c, getAnalysis<ObjectID>());
					errs() << "\n";
					constraints.push_back(c);
				}
			}
		}
#endif
	} else if (n_overwriters == 1) {
		// TODO: path_may_write only checks the current thread. Need check
		// other threads as well. 
		if (!path_may_write(
					latest_overwriters[the_thr_idx], latest_doms[the_thr_idx], q)) {
			Clause *c = new Clause(new BoolExpr(
						CmpInst::ICMP_EQ,
						new Expr(i2),
						new Expr(get_value_operand(latest_overwriters[the_thr_idx]))));
			DEBUG(dbgs() << "From overwriting: ";
			print_clause(dbgs(), c, getAnalysis<ObjectID>());
			dbgs() << "\n";);
			constraints.push_back(c);
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
#if 0
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
#if 0
	errs() << "get_idom_ip:" << *ins << "\n";
#endif
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
