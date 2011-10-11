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
#include "llvm/Support/CommandLine.h"
#include "common/callgraph-fp.h"
#include "common/icfg.h"
#include "common/intra-reach.h"
#include "common/exec-once.h"
#include "common/partial-icfg-builder.h"
#include "common/reach.h"
using namespace llvm;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "slicer/capture.h"
#include "slicer/must-alias.h"
#include "slicer/adv-alias.h"
#include "slicer/landmark-trace.h"
#include "slicer/clone-info-manager.h"
#include "slicer/region-manager.h"
using namespace slicer;

static cl::opt<bool> DisableAdvancedAA("disable-advanced-aa",
		cl::desc("Don't use the advanced AA. Always use bc2bdd"));

Value *CaptureConstraints::get_pointer_operand(const Instruction *i) {
	if (const StoreInst *si = dyn_cast<StoreInst>(i))
		return si->getOperand(1);
	if (const LoadInst *li = dyn_cast<LoadInst>(i))
		return li->getOperand(0);
	return NULL;
}

Value *CaptureConstraints::get_value_operand(const Instruction *i) {
	if (const StoreInst *si = dyn_cast<StoreInst>(i))
		return si->getOperand(0);
	if (const LoadInst *li = dyn_cast<LoadInst>(i))
		return const_cast<LoadInst *>(li);
	return NULL;
}

void CaptureConstraints::capture_addr_taken(Module &M) {
	TimerGroup tg("Capture constraints on address-taken variables");

#if 0
	Timer tmr_may_assign("may-assign", tg);
	tmr_may_assign.startTimer();
	capture_may_assign(M);
	tmr_may_assign.stopTimer();
#endif
	
	Timer tmr_must_assign("must-assign", tg);
	tmr_must_assign.startTimer();
	capture_must_assign(M);
	tmr_must_assign.stopTimer();

	Timer tmr_global_var("global-var", tg);
	tmr_global_var.startTimer();
	capture_global_vars(M);
	tmr_global_var.stopTimer();
}

void CaptureConstraints::capture_global_vars(Module &M) {
	for (Module::global_iterator gi = M.global_begin();
			gi != M.global_end(); ++gi) {
		if (isa<IntegerType>(gi->getType()) || isa<PointerType>(gi->getType()))
			capture_global_var(gi);
	}
}

void CaptureConstraints::capture_global_var(GlobalVariable *gv) {
	RegionManager &RM = getAnalysis<RegionManager>();
	BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();

	assert(isa<IntegerType>(gv->getType()) || isa<PointerType>(gv->getType()));

	DenseMap<Region, ConstValueList> overwriting_regions;
	// Consider <gv>'s initializer as well. 
	if (gv->hasInitializer()) {
		overwriting_regions[Region(0, (size_t)-1, 0)].push_back(
				gv->getInitializer());
	}
	Module *m = gv->getParent();
	for (Module::iterator f = m->begin(); f != m->end(); ++f) {
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				if (StoreInst *si = dyn_cast<StoreInst>(ins)) {
					if (BAA.alias(gv, 0, si->getPointerOperand(), 0)) {
						vector<Region> regions;
						RM.get_containing_regions(si, regions);
						for (size_t i = 0; i < regions.size(); ++i)
							overwriting_regions[regions[i]].push_back(si->getOperand(0));
					}
				}
				// TODO: summary, and use may_write functions etc. 
				CallSite cs = CallSite::get(ins);
				if (cs.getInstruction()) {
					Function *callee = cs.getCalledFunction();
					if (callee && callee->getName() == "fscanf") {
						assert(cs.arg_size() > 0);
						if (BAA.alias(gv, 0, cs.getArgument(cs.arg_size() - 1), 0)) {
							vector<Region> regions;
							RM.get_containing_regions(ins, regions);
							for (size_t i = 0; i < regions.size(); ++i)
								overwriting_regions[regions[i]].push_back(NULL);
						}
					}
				}
			}
		}
	}

	vector<Region> to_be_removed;
	for (DenseMap<Region, ConstValueList>::iterator
			i = overwriting_regions.begin(); i != overwriting_regions.end(); ++i) {
		bool happens_before = false;
		for (DenseMap<Region, ConstValueList>::iterator
				j = overwriting_regions.begin(); j != overwriting_regions.end(); ++j) {
			if (RM.happens_before(i->first, j->first)) {
				happens_before = true;
				break;
			}
		}
		if (happens_before)
			to_be_removed.push_back(i->first);
	}
	for (size_t i = 0; i < to_be_removed.size(); ++i)
		overwriting_regions.erase(to_be_removed[i]);
	
	InstList equivalent_loads;
	for (Value::use_iterator ui = gv->use_begin(); ui != gv->use_end(); ++ui) {
		if (LoadInst *li = dyn_cast<LoadInst>(*ui)) {
			vector<Region> containing_regions;
			RM.get_containing_regions(li, containing_regions);
			bool all_happens_after = true;
			for (size_t i = 0; i < containing_regions.size(); ++i) {
				for (DenseMap<Region, ConstValueList>::iterator
						j = overwriting_regions.begin();
						j != overwriting_regions.end(); ++j) {
					if (!RM.happens_before(j->first, containing_regions[i])) {
						all_happens_after = false;
						break;
					}
				}
				if (!all_happens_after)
					break;
			}
			if (all_happens_after)
				equivalent_loads.push_back(li);
		}
	}

	for (size_t i = 0; i < equivalent_loads.size(); ++i)
		fixed_integers.insert(equivalent_loads[i]);
	for (size_t i = 0; i + 1 < equivalent_loads.size(); ++i) {
		add_constraint(new Clause(new BoolExpr(CmpInst::ICMP_EQ,
						new Expr(equivalent_loads[i]), new Expr(equivalent_loads[i + 1]))));
	}
	if (equivalent_loads.size() > 0) {
		Clause *disj = NULL;
		for (DenseMap<Region, ConstValueList>::iterator
				i = overwriting_regions.begin(); i != overwriting_regions.end(); ++i) {
			bool may_equal_unknown = false;
			for (size_t k = 0; k < i->second.size(); ++k) {
				if (i->second[k] == NULL) {
					may_equal_unknown = true;
					break;
				}
			}
			if (may_equal_unknown) {
				delete disj;
				disj = NULL;
				break;
			}
			for (size_t k = 0; k < i->second.size(); ++k) {
				Clause *c = new Clause(new BoolExpr(CmpInst::ICMP_EQ,
							new Expr(equivalent_loads[0]),
							new Expr(i->second[k])));
				if (!disj)
					disj = c;
				else
					disj = new Clause(Instruction::Or, disj, c);
			}
		}
		add_constraint(disj);
	}
}

#if 0
void CaptureConstraints::capture_may_assign(Module &M) {
	ExecOnce &EO = getAnalysis<ExecOnce>();

	// Find all loads and stores. 
	vector<const LoadInst *> all_loads;
	// <value, pointer>
	// <all_stores> includes global variable initializers as well. 
	// Therefore, not always a StoreInst. 
	vector<pair<const Value *, const Value *> > all_stores;
	// Scan through all loads and stores. 
	// NOTE: We don't guarantee these stores and loads are constant. 
	// Actually, we need consider non-constant stores here, because any
	// instruction that loads from it may end up loading anything. 
	forallinst(M, ii) {
		if (EO.not_executed(ii))
			continue;
		Value *v = get_value_operand(ii);
		if (!v)
			continue;
		if (isa<IntegerType>(v->getType()) || isa<PointerType>(v->getType())) {
			if (StoreInst *si = dyn_cast<StoreInst>(ii))
				all_stores.push_back(make_pair(v, si->getPointerOperand()));
			if (LoadInst *li = dyn_cast<LoadInst>(ii))
				all_loads.push_back(li);
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
			// TODO: To handle ConstantAggregateZero
			if (ConstantInt *ci = dyn_cast<ConstantInt>(gi->getInitializer()))
				all_stores.push_back(make_pair(ci, gi));
		}
	}
	dbgs() << "=== Capturing may assignments === ";
	dbgs() << "loads = " << all_loads.size() << "; ";
	dbgs() << "stores = " << all_stores.size() << "\n";

	for (size_t i = 0; i < all_loads.size(); ++i) {
		print_progress(dbgs(), i, all_loads.size());

		// TODO: Capture constant => non-constant assignments as well. 
		if (!EO.executed_once(all_loads[i]))
			continue;

		Clause *disj = NULL;
		unsigned n_terms = 0;
		for (size_t j = 0; j < all_stores.size(); ++j) {
			if (may_alias(all_loads[i]->getPointerOperand(), all_stores[j].second)) {
				// If the stored value is not constant, the loaded value
				// can be anything. So, no constraint will be captured in
				// this case. 
				if (const Instruction *si = dyn_cast<Instruction>(all_stores[j].first)) {
					if (!EO.executed_once(si)) {
						// dbgs() << "[Warning] Stores a variable\n";
						if (disj) {
							delete disj;
							disj = NULL;
						}
						break;
					}
				}
				Clause *c = new Clause(new BoolExpr(
							CmpInst::ICMP_EQ,
							new Expr(all_loads[i]),
							new Expr(all_stores[j].first)));
				if (!disj)
					disj = c;
				else {
					assert(disj != c);
					disj = new Clause(Instruction::Or, disj, c);
				}
				++n_terms;
			}
		} // for store
#if 0
		if (n_terms != 1 && disj) {
			delete disj;
			disj = NULL;
		}
#endif
		add_constraint(disj);
	}

	// Finish the progress bar. 
	print_progress(dbgs(), all_loads.size(), all_loads.size());
	dbgs() << "\n";
}
#endif

void CaptureConstraints::capture_must_assign(Module &M) {
	ExecOnce &EO = getAnalysis<ExecOnce>();

	unsigned n_loads = 0;
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		if (!f->isDeclaration() && !EO.not_executed(f)) {
			forall(Function, bb, *f) {
				forall(BasicBlock, ins, *bb) {
					if (isa<LoadInst>(ins))
						++n_loads;
				}
			}
		}
	}
	dbgs() << "=== Capturing must assignments === ";
	dbgs() << "# of loads = " << n_loads << "\n";

	unsigned cur = 0;
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		if (f->isDeclaration())
			continue;
		if (EO.not_executed(f))
			continue;
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				if (LoadInst *i2 = dyn_cast<LoadInst>(ins)) {
					print_progress(dbgs(), cur, n_loads);
					const Type *i2_type = i2->getType();
					// We don't capture equalities on real numbers. 
					if (isa<IntegerType>(i2_type) || isa<PointerType>(i2_type))
						capture_overwriting_to(i2);
					++cur;
				}
			}
		}
	}
	
	// Finish the progress bar. 
	print_progress(dbgs(), n_loads, n_loads);
	dbgs() << "\n";
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
	// Due to function inlining, some instructions with clone_info are 
	// actually unreachable. 
	ICFGNode *n1 = PIB[m1], *n2 = PIB[m2];
	if (n1 == NULL)
		return i2;
	if (n2 == NULL)
		return i1;

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
		if (Value *p = get_pointer_operand(i1)) {
			if (must_alias(p, q))
				break;
		}
		i1 = get_idom_ip(i1);
	}
	return i1;
}

bool CaptureConstraints::region_may_write(const Region &r, const Value *q) {
	RegionManager &RM = getAnalysis<RegionManager>();

	if (!RM.region_has_insts(r))
		return false;

	const ConstInstList &insts_in_r = RM.get_insts_in_region(r);
	for (size_t i = 0; i < insts_in_r.size(); ++i) {
		// TODO: Trace into functions. 
		if (const StoreInst *si = dyn_cast<StoreInst>(insts_in_r[i])) {
			DEBUG(dbgs() << "region_may_write?" << *si << "\n";);
			if (may_alias(si->getPointerOperand(), q)) {
				DEBUG(dbgs() << "region_may_write:" << *si << "\n";);
				return true;
			}
		}
	}

	return false;
}

void CaptureConstraints::capture_overwriting_to(LoadInst *i2) {
	LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
	RegionManager &RM = getAnalysis<RegionManager>();
	ExecOnce &EO = getAnalysis<ExecOnce>();

	Value *q = i2->getPointerOperand();
	// We only handle the case that <i2> is executed once for now. 
	// TODO: Change to is_fixed_integer? 
	if (!EO.executed_once(i2))
		return;
	
	vector<Region> cur_regions;
	RM.get_containing_regions(i2, cur_regions);
	if (cur_regions.size() != 1)
		return;
#if 0
	errs() << "cur_region = " << cur_regions[0] << "\n";
#endif
	int cur_thr_id = cur_regions[0].thr_id;
	size_t prev_enforcing = cur_regions[0].prev_enforcing_landmark;
	// The preparer instruments the entry of each thread function
	// (including main). Therefore, we can always find a previous enforcing
	// landmark. 
	assert(prev_enforcing != (size_t)-1);
	
	DEBUG(dbgs() << "capture_overwriting_to (vid = " <<
			getAnalysis<IDAssigner>().getValueID(i2) << "): " << cur_thr_id <<
			' ' << prev_enforcing << ":" << *i2 << "\n";);

	// If any store is concurrent with cur_regions[0], return
	vector<Region> concurrent_regions;
	RM.get_concurrent_regions(cur_regions[0], concurrent_regions);
	DEBUG(dbgs() << "# of concurrent regions = " <<
			concurrent_regions.size() << "\n");
	for (size_t i = 0; i < concurrent_regions.size(); ++i) {
		if (region_may_write(concurrent_regions[i], q)) {
			DEBUG(dbgs() << concurrent_regions[i] <<
					" may write to this pointer\n";);
			return;
		}
	}

	// Compute the latest dominator in each thread. 
	vector<int> thr_ids = LT.get_thr_ids();
	vector<Instruction *> latest_doms;
	vector<size_t> latest_preceeding_enforcing_landmarks;
	for (size_t k = 0; k < thr_ids.size(); ++k) {
		int i = thr_ids[k];
		if (i == cur_thr_id) {
			latest_preceeding_enforcing_landmarks.push_back(prev_enforcing);
			latest_doms.push_back(i2);
			continue;
		}
		
		assert(prev_enforcing != (size_t)-1);
		size_t j = LT.get_latest_happens_before(cur_thr_id, prev_enforcing, i);
		latest_preceeding_enforcing_landmarks.push_back(j);
		if (j == (size_t)-1) {
			latest_doms.push_back(NULL);
			continue;
		}
		
		// TraceManager is still looking at the trace for the original program.
		// But, we should use the cloned instruction.
		unsigned orig_ins_id = LT.get_landmark(i, j).ins_id;
		const InstList &landmarks = CIM.get_instructions(i, j, orig_ins_id);
		assert(landmarks.size() > 0);
		Instruction *latest_dom = NULL;
		for (size_t t = 0; t < landmarks.size(); ++t) {
			if (EO.not_executed(landmarks[t]))
				continue;
			latest_dom = find_nearest_common_dom(latest_dom, landmarks[t]);
		}
		if (!latest_dom) {
			errs() << "get_landmark: " << i << ' ' << j <<
				' ' << orig_ins_id << "\n";
		}
		assert(latest_dom && "Cannot find the cloned landmark");
		latest_doms.push_back(latest_dom);
	}

	assert(latest_doms.size() == thr_ids.size());
	assert(latest_preceeding_enforcing_landmarks.size() == thr_ids.size());

	// Compute the latest overwriter of the latest dominator in each thread. 
	vector<Instruction *> latest_overwriters;
	vector<Region> overwriter_regions;
	for (size_t k = 0; k < thr_ids.size(); ++k) {
		if (!latest_doms[k]) {
			latest_overwriters.push_back(NULL);
			overwriter_regions.push_back(DenseMapInfo<Region>::getTombstoneKey());
			continue;
		}
		Instruction *latest_overwriter = find_latest_overwriter(latest_doms[k], q);
		vector<Region> regions;
		if (latest_overwriter)
			RM.get_containing_regions(latest_overwriter, regions);
		if (regions.size() != 1) {
			latest_overwriters.push_back(NULL);
			overwriter_regions.push_back(DenseMapInfo<Region>::getTombstoneKey());
		} else {
			latest_overwriters.push_back(latest_overwriter);
			overwriter_regions.push_back(regions[0]);
		}
	}
	assert(latest_overwriters.size() == thr_ids.size());
	assert(overwriter_regions.size() == thr_ids.size());

	// These latest overwriters may not all be the valid sources of
	// this LoadInst. Compare their containing regions, and prune out
	// those which cannot. 
	for (size_t k1 = 0; k1 < thr_ids.size(); ++k1) {
		if (!latest_overwriters[k1])
			continue;
		bool before = false;
		for (size_t k2 = 0; k2 < thr_ids.size(); ++k2) {
			if (!latest_overwriters[k2] || k1 == k2)
				continue;
			// If latest_overwriters[k1] must happen before latest_overwriters[k2], 
			// remove latest_overwriters[k1]. 
			if (RM.happens_before(overwriter_regions[k1], overwriter_regions[k2])) {
				before = true;
				break;
			}
		}
		if (before)
			latest_overwriters[k1] = NULL;
	}

	unsigned n_overwriters = 0;
	for (size_t k = 0; k < thr_ids.size(); ++k) {
		if (!latest_overwriters[k])
			continue; // ignore this overwriter
		/*
		 * If the path from latest_overwriters[k] to
		 * latest_preceeding_enforcing_landmarks[k] may write to <q>, 
		 * it may not be a valid overwriter. 
		 */
		if (thr_ids[k] == cur_thr_id) {
			if (path_may_write(latest_overwriters[k], i2, q))
				continue;
		} else {
			if (path_may_write(latest_overwriters[k], thr_ids[k],
						latest_preceeding_enforcing_landmarks[k], q))
				continue; // ignore this overwriter
		}
		/*
		 * If any store is concurrent with regions containing the path
		 * from latest_overwriters[k] to <prev_enforcing>,
		 * it may not be a valid overwriter. 
		 */
		DenseSet<Region> concurrent_regions;
		Region r = overwriter_regions[k];
		while (r.prev_enforcing_landmark !=
				latest_preceeding_enforcing_landmarks[k]) {
			vector<Region> concurrent_regions_of_r;
			RM.get_concurrent_regions(r, concurrent_regions_of_r);
			concurrent_regions.insert(concurrent_regions_of_r.begin(),
					concurrent_regions_of_r.end());
			r = RM.next_region(r);
		}
		if (r.thr_id != cur_thr_id) {
			r = RM.prev_region(r);
			r = RM.next_region_in_thread(r, cur_thr_id);
			while (r.prev_enforcing_landmark != prev_enforcing) {
				vector<Region> concurrent_regions_of_r;
				RM.get_concurrent_regions(r, concurrent_regions_of_r);
				concurrent_regions.insert(concurrent_regions_of_r.begin(),
						concurrent_regions_of_r.end());
				r = RM.next_region(r);
			}
		}
		
		// TODO: Cache the region_may_write results. 
		bool overwritten_by_concurrent_regions = false;
		forall(DenseSet<Region>, it, concurrent_regions) {
			if (region_may_write(*it, q)) {
				overwritten_by_concurrent_regions = true;
				break;
			}
		}
		if (overwritten_by_concurrent_regions)
			continue; // ignore this overwriter

		Clause *c = new Clause(new BoolExpr(
					CmpInst::ICMP_EQ,
					new Expr(i2),
					new Expr(get_value_operand(latest_overwriters[k]))));
#if 0
		errs() << *latest_doms[k] << "\n";
#endif
		DEBUG(dbgs() << "Valid overwriter:" << *latest_overwriters[k] << "\n";);
		DEBUG(dbgs() << "From overwriting: ";
				print_clause(dbgs(), c, getAnalysis<IDAssigner>());
				dbgs() << "\n";);
		add_constraint(c);
		n_overwriters++;
	}
	DEBUG(dbgs() << "# of overwriters = " << n_overwriters << "\n";);
}

bool CaptureConstraints::may_write(
		const Instruction *i, const Value *q, ConstFuncSet &visited_funcs) {
	if (const StoreInst *si = dyn_cast<StoreInst>(i)) {
		if (may_alias(si->getPointerOperand(), q))
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

bool CaptureConstraints::path_may_write(int thr_id, size_t trunk_id,
		const Instruction *i2, const Value *q) {
	LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
	MicroBasicBlockBuilder &MBBB = getAnalysis<MicroBasicBlockBuilder>();
	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
	ICFG &PIB = getAnalysis<PartialICFGBuilder>();
	ExecOnce &EO = getAnalysis<ExecOnce>();

	assert(trunk_id != (size_t)-1);
	assert(LT.is_enforcing_landmark(thr_id, trunk_id));

	unsigned orig_ins_id = LT.get_landmark(thr_id, trunk_id).ins_id;
	const InstList &landmarks = CIM.get_instructions(
			thr_id, trunk_id, orig_ins_id);
	DenseSet<const ICFGNode *> sink;
	for (size_t i = 0; i < landmarks.size(); ++i) {
		if (EO.not_executed(landmarks[i]))
			continue;
		MicroBasicBlock *m1 = MBBB.parent(landmarks[i]); assert(m1);
		ICFGNode *n1 = PIB[m1]; assert(n1);
		sink.insert(n1);
	}

	MicroBasicBlock *m2 = MBBB.parent(i2); assert(m2);
	ICFGNode *n2 = PIB[m2]; assert(n2);
	
	Reach<ICFGNode> IR;
	DenseSet<const ICFGNode *> visited;
	IR.floodfill_r(n2, sink, visited);
	bool reached_sink = false;
	forall(DenseSet<const ICFGNode *>, it, sink) {
		if (visited.count(*it)) {
			reached_sink = true;
			break;
		}
	}
	assert(reached_sink && "<i1> should dominate <i2>");

	return blocks_may_write(visited, landmarks,
			InstList(1, const_cast<Instruction *>(i2)), q);
}

bool CaptureConstraints::path_may_write(const Instruction *i1,
		int thr_id, size_t trunk_id, const Value *q) {

#if 0
	errs() << "path_may_write:" << *i1 << "\n";
	errs() << thr_id << " " << trunk_id << "\n";
#endif

	LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
	MicroBasicBlockBuilder &MBBB = getAnalysis<MicroBasicBlockBuilder>();
	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
	ICFG &PIB = getAnalysis<PartialICFGBuilder>();
	ExecOnce &EO = getAnalysis<ExecOnce>();

	assert(trunk_id != (size_t)-1);
	assert(LT.is_enforcing_landmark(thr_id, trunk_id));

	unsigned orig_ins_id = LT.get_landmark(thr_id, trunk_id).ins_id;
	const InstList &landmarks = CIM.get_instructions(
			thr_id, trunk_id, orig_ins_id);
	DenseSet<const ICFGNode *> sink;
	for (size_t i = 0; i < landmarks.size(); ++i) {
		if (EO.not_executed(landmarks[i]))
			continue;
		MicroBasicBlock *m2 = MBBB.parent(landmarks[i]); assert(m2);
		ICFGNode *n2 = PIB[m2]; assert(n2);
		sink.insert(n2);
	}

	MicroBasicBlock *m1 = MBBB.parent(i1); assert(m1);
	ICFGNode *n1 = PIB[m1]; assert(n1);
	
	Reach<ICFGNode> IR;
	DenseSet<const ICFGNode *> visited;
	IR.floodfill(n1, sink, visited);
	bool reached_sink = false;
	forall(DenseSet<const ICFGNode *>, it, sink) {
		if (visited.count(*it)) {
			reached_sink = true;
			break;
		}
	}
	assert(reached_sink && "<i2> should post-dominate <i1>");

	return blocks_may_write(visited,
			InstList(1, const_cast<Instruction *>(i1)),
			landmarks, q);
}

bool CaptureConstraints::blocks_may_write(
		const DenseSet<const ICFGNode *> &blocks,
		const InstList &starts, const InstList &ends, const Value *q) {

	// Functions visited in <may_write>s. 
	// In order to handle recursive functions. 
	// FIXME: Trace into functions that don't appear in the ICFG. 
	forallconst(DenseSet<const ICFGNode *>, it, blocks) {
		const MicroBasicBlock *mbb = (*it)->getMBB();
		BasicBlock::const_iterator s = mbb->end();
		while (s != mbb->begin()) {
			--s;
			if (find(starts.begin(), starts.end(), s) != starts.end()) {
				++s;
				break;
			}
		}
		BasicBlock::const_iterator e = mbb->begin();
		while (e != mbb->end()) {
			if (find(ends.begin(), ends.end(), e) != ends.end())
				break;
			++e;
		}
		for (BasicBlock::const_iterator i = s; i != e; ++i) {
			if (const StoreInst *si = dyn_cast<StoreInst>(i)) {
				if (may_alias(si->getPointerOperand(), q))
					return true;
			}
		}
	}
 	return false;
}

bool CaptureConstraints::path_may_write(
		const Instruction *i1, const Instruction *i2, const Value *q) {

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
				if (may_alias(si->getPointerOperand(), q))
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

bool CaptureConstraints::may_alias(const Value *v1, const Value *v2) {
	AdvancedAlias *AAA = getAnalysisIfAvailable<AdvancedAlias>();
	if (!DisableAdvancedAA && AAA)
		return AAA->may_alias(v1, v2);
	else {
		BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
		return BAA.alias(v1, 0, v2, 0) == AliasAnalysis::MayAlias;
	}
}

bool CaptureConstraints::must_alias(const Value *v1, const Value *v2) {
	AdvancedAlias *AAA = getAnalysisIfAvailable<AdvancedAlias>();
	if (!DisableAdvancedAA && AAA)
		return AAA->must_alias(v1, v2);
	else
		return v1 == v2;
}

bool CaptureConstraints::is_using_advanced_alias() {
	return getAnalysisIfAvailable<AdvancedAlias>();
}
