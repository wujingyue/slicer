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
using namespace bc2bdd;

#include "slicer/capture.h"
#include "slicer/adv-alias.h"
#include "slicer/landmark-trace.h"
#include "slicer/clone-info-manager.h"
#include "slicer/region-manager.h"
#include "slicer/may-write-analyzer.h"
using namespace slicer;

static cl::opt<bool> DisableAdvancedAA("disable-advanced-aa",
		cl::desc("Don't use the advanced AA. Always use bc2bdd"));
static cl::opt<bool> DisableAddressTaken("disable-addr-taken",
		cl::desc("Don't capture constraints on address-taken variables"));
static cl::opt<bool> Verbose("verbose",
		cl::desc("Print information for each alias query"));

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
	MayWriteAnalyzer &MWA = getAnalysis<MayWriteAnalyzer>();

	assert(isa<IntegerType>(gv->getType()) || isa<PointerType>(gv->getType()));

	// key: an overwriting region
	// value: the value stored. NULL if unknown. 
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
				ConstFuncSet visited_funcs;
				if (MWA.may_write(ins, gv, visited_funcs, false)) {
					vector<Region> regions;
					RM.get_containing_regions(ins, regions);
					for (size_t i = 0; i < regions.size(); ++i) {
						Value *the_value = NULL;
						if (StoreInst *si = dyn_cast<StoreInst>(ins))
							the_value = si->getValueOperand();
						overwriting_regions[regions[i]].push_back(the_value);
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
			if (find(i->second.begin(), i->second.end(), (const Value *)NULL) !=
					i->second.end()) {
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

void CaptureConstraints::capture_must_assign(Module &M) {
	ExecOnce &EO = getAnalysis<ExecOnce>();

	// Compute <n_loads> in order to print the progress bar. 
	unsigned n_loads = 0;
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		if (!f->isDeclaration() && !EO.not_executed(f)) {
			for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
				for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
					if (isa<LoadInst>(ins)) {
						const Type *ty = ins->getType();
						if (isa<IntegerType>(ty) || isa<PointerType>(ty))
							++n_loads;
					}
				}
			}
		}
	}
	dbgs() << "=== Capturing must assignments === ";
	dbgs() << "# of loads = " << n_loads << "\n";

	unsigned cur_load = 0;
	unsigned n_captured = 0, n_uncaptured = 0;
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		if (f->isDeclaration())
			continue;
		if (EO.not_executed(f))
			continue;
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				if (LoadInst *i2 = dyn_cast<LoadInst>(ins)) {
					const Type *i2_type = i2->getType();
					// We don't capture equalities on real numbers. 
					if (isa<IntegerType>(i2_type) || isa<PointerType>(i2_type)) {
						print_progress(dbgs(), cur_load, n_loads);
						bool captured = capture_overwriting_to(i2);
						++(captured ? n_captured : n_uncaptured);
						++cur_load;
					}
				}
			}
		}
	}
	assert(n_loads == n_captured + n_uncaptured);
	
	// Finish the progress bar. 
	print_progress(dbgs(), n_loads, n_loads);
	dbgs() << "\n";
	dbgs() << "# of captured loads = " << n_captured
		<< "; # of uncaptured loads = " << n_uncaptured << "\n";
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
	DEBUG(dbgs() << "find_latest_ovewriter:" << *i2 << "\n";
			dbgs() << *q << "\n";);

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
	if (i1)
		DEBUG(dbgs() << "Found:" << *i1 << "\n");
	return i1;
}

bool CaptureConstraints::region_may_write(const Region &r, const Value *q) {
	RegionManager &RM = getAnalysis<RegionManager>();
	MayWriteAnalyzer &MWA = getAnalysis<MayWriteAnalyzer>();

	if (!RM.region_has_insts(r))
		return false;

	const ConstInstList &insts_in_r = RM.get_insts_in_region(r);
	// A region already includes exec-once functions. 
	ConstFuncSet visited_funcs;
	for (size_t i = 0; i < insts_in_r.size(); ++i) {
		if (MWA.may_write(insts_in_r[i], q, visited_funcs)) {
			DEBUG(dbgs() << "Region " << r << "may write to <q>:\n";);
			DEBUG(dbgs() << *insts_in_r[i] << "\n";);
			return true;
		}
	}

	return false;
}

void CaptureConstraints::add_constraints(const vector<Clause *> &cs) {
	for (size_t i = 0; i < cs.size(); ++i)
		add_constraint(cs[i]);
}

bool CaptureConstraints::capture_overwriting_to(LoadInst *i2) {
	DEBUG(dbgs() << "** capture_overwriting_to:" << *i2 << "\n";);
	DEBUG(dbgs() << "vid = " << getAnalysis<IDAssigner>().getValueID(i2) << "\n";);

	// Check cache. 
	if (captured_loads.count(i2)) {
		DEBUG(dbgs() << "cache hit\n";);
		vector<Clause *> to_be_added = captured_loads.lookup(i2);
		for (size_t i = 0; i < to_be_added.size(); ++i) {
			DEBUG(dbgs() << "cached constraint: ";);
			DEBUG(print_clause(dbgs(), to_be_added[i], getAnalysis<IDAssigner>()));
			DEBUG(dbgs() << "\n";);
			add_constraint(to_be_added[i]->clone());
		}
		return true;
	}

	LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
	RegionManager &RM = getAnalysis<RegionManager>();
	ExecOnce &EO = getAnalysis<ExecOnce>();

	// We only handle the case that <i2> is executed once for now. 
	// TODO: Change to is_fixed_integer? 
	if (!EO.executed_once(i2)) {
		DEBUG(dbgs() << "will not be executed. give up\n";);
		return false;
	}
	
	vector<Region> cur_regions;
	RM.get_containing_regions(i2, cur_regions);
	if (cur_regions.size() != 1) {
		DEBUG(dbgs() << "contained in multiple regions. give up\n");
		return false;
	}
	DEBUG(dbgs() << "containing region = " << cur_regions[0] << "\n";);

	int cur_thr_id = cur_regions[0].thr_id;
	size_t prev_enforcing = cur_regions[0].prev_enforcing_landmark;
	// The preparer instruments the entry of each thread function
	// (including main). Therefore, we can always find a previous enforcing
	// landmark. 
	assert(prev_enforcing != (size_t)-1);
	
	if (Verbose)
		dbgs() << "|";

	// If any store is concurrent with cur_regions[0], return
	Value *q = i2->getPointerOperand();
	vector<Region> concurrent_regions;
	RM.get_concurrent_regions(cur_regions[0], concurrent_regions);
	for (size_t i = 0; i < concurrent_regions.size(); ++i) {
		if (region_may_write(concurrent_regions[i], q)) {
			DEBUG(dbgs() << concurrent_regions[i] <<
					" may write to this pointer\n";);
			return false;
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

	vector<Clause *> final_constraints;
	for (size_t k = 0; k < thr_ids.size(); ++k) {
		if (!latest_overwriters[k])
			continue; // ignore this overwriter
		DEBUG(dbgs() << "potential def:" << *latest_overwriters[k] << "\n";);
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
		for (DenseSet<Region>::iterator it = concurrent_regions.begin();
				it != concurrent_regions.end(); ++it) {
			DEBUG(dbgs() << "killed by " << *it << "\n";);
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
		final_constraints.push_back(c);

		DEBUG(dbgs() << "valid def:" << *latest_overwriters[k] << "\n";);
		DEBUG(dbgs() << "constraint: ";);
		DEBUG(print_clause(dbgs(), c, getAnalysis<IDAssigner>()););
		DEBUG(dbgs() << "\n";);
	}

	DEBUG(dbgs() << "# of valid defs = " << final_constraints.size() << "\n";);
	if (final_constraints.empty())
		return false;

	if (DisableAddressTaken)
		final_constraints.clear();
	captured_loads[i2] = final_constraints;
	for (size_t i = 0; i < final_constraints.size(); ++i)
		add_constraint(final_constraints[i]->clone());
	return true;
}

bool CaptureConstraints::path_may_write(int thr_id, size_t trunk_id,
		const Instruction *i2, const Value *q) {
	DEBUG(dbgs() << "path_may_write:" << *i2 << "\n";
			dbgs() << thr_id << " " << trunk_id << "\n";);

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

	return mbbs_may_write(visited, landmarks,
			InstList(1, const_cast<Instruction *>(i2)), q);
}

bool CaptureConstraints::path_may_write(const Instruction *i1,
		int thr_id, size_t trunk_id, const Value *q) {
	DEBUG(dbgs() << "path_may_write:" << *i1 << "\n";
			dbgs() << thr_id << " " << trunk_id << "\n";);

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

	return mbbs_may_write(visited,
			InstList(1, const_cast<Instruction *>(i1)),
			landmarks, q);
}

bool CaptureConstraints::mbbs_may_write(const DenseSet<const ICFGNode *> &mbbs,
		const InstList &starts, const InstList &ends, const Value *q) {
	for (DenseSet<const ICFGNode *>::const_iterator it = mbbs.begin();
			it != mbbs.end(); ++it) {
		const MicroBasicBlock *mbb = (*it)->getMBB();
		if (mbb_may_write(mbb, starts, ends, q))
			return true;
	}

	return false;
}

bool CaptureConstraints::mbb_may_write(const MicroBasicBlock *mbb,
		const InstList &starts, const InstList &ends, const Value *q) {
	MayWriteAnalyzer &MWA = getAnalysis<MayWriteAnalyzer>();

	// The starting point may not be the entry of <mbb>. It may be one of the
	// instructions in <starts>. 
	BasicBlock::const_iterator s = mbb->end();
	while (s != mbb->begin()) {
		--s;
		if (find(starts.begin(), starts.end(), s) != starts.end()) {
			++s;
			break;
		}
	}
	
	// The ending point may not be the exit of <mbb>. It may be one of the
	// instructions in <ends>. 
	BasicBlock::const_iterator e = mbb->begin();
	while (e != mbb->end()) {
		if (find(ends.begin(), ends.end(), e) != ends.end())
			break;
		++e;
	}

	// Trace into functions that don't appear in the ICFG. 
	ConstFuncSet visited_funcs;
	for (BasicBlock::const_iterator i = s; i != e; ++i) {
		if (MWA.may_write(i, q, visited_funcs))
			return true;
	}
 	
	return false;
}

bool CaptureConstraints::path_may_write(const Instruction *i1,
		const Instruction *i2, const Value *q) {
	DEBUG(dbgs() << "path_may_write:\n");
	DEBUG(dbgs() << "  start: ";);
	DEBUG(dbgs() << "[" << i1->getParent()->getParent()->getName() << "]";);
	DEBUG(dbgs() << *i1 << "\n";);
	DEBUG(dbgs() << "  end: ";);
	DEBUG(dbgs() << "[" << i2->getParent()->getParent()->getName() << "]";);
	DEBUG(dbgs() << *i2 << "\n";);

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

	return mbbs_may_write(visited,
			InstList(1, const_cast<Instruction *>(i1)),
			InstList(1, const_cast<Instruction *>(i2)), q);
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
	if (!DisableAdvancedAA && AAA) {
		bool res = AAA->may_alias(v1, v2);
		if (Verbose)
			dbgs() << (res ? "A" : "a");
		return res;
	} else {
		AliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
		return BAA.alias(v1, 0, v2, 0) == AliasAnalysis::MayAlias;
	}
}

bool CaptureConstraints::must_alias(const Value *v1, const Value *v2) {
	AdvancedAlias *AAA = getAnalysisIfAvailable<AdvancedAlias>();
	if (!DisableAdvancedAA && AAA) {
		bool res = AAA->must_alias(v1, v2);
		if (Verbose)
			dbgs() << (res ? "U" : "u");
		return res;
	} else {
		return v1 == v2;
	}
}

bool CaptureConstraints::is_using_advanced_alias() {
	return getAnalysisIfAvailable<AdvancedAlias>();
}
