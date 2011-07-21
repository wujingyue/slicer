/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "region-manager"

#include "llvm/Support/Debug.h"
#include "common/cfg/partial-icfg-builder.h"
#include "common/cfg/reach.h"
#include "common/callgraph-fp/callgraph-fp.h"
#include "idm/mbb.h"
using namespace llvm;

#include "region-manager.h"
#include "clone-info-manager.h"
#include "max-slicing.h"
#include "trace/landmark-trace.h"
using namespace slicer;

static RegisterPass<RegionManager> X(
		"manage-region",
		"Mark each cloned instruction with its previous enforcing landmark "
		"and next enforcing landmark",
		false,
		true);

char RegionManager::ID = 0;

void RegionManager::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<CloneInfoManager>();
	AU.addRequired<LandmarkTrace>();
	AU.addRequired<MicroBasicBlockBuilder>();
	AU.addRequired<PartialICFGBuilder>();
	AU.addRequiredTransitive<CallGraphFP>();
	ModulePass::getAnalysisUsage(AU);
}

bool slicer::operator<(const Region &a, const Region &b) {
	return a.thr_id < b.thr_id || (a.thr_id == b.thr_id &&
			b.next_enforcing_landmark < b.next_enforcing_landmark);
}

bool slicer::operator==(const Region &a, const Region &b) {
	return a.thr_id == b.thr_id &&
		a.prev_enforcing_landmark == b.prev_enforcing_landmark;
}

raw_ostream &slicer::operator<<(raw_ostream &O, const Region &r) {
	O << "(" << r.thr_id << ", " << r.prev_enforcing_landmark << ", " <<
		r.next_enforcing_landmark << ")";
	return O;
}

bool RegionManager::runOnModule(Module &M) {

	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
	LandmarkTrace &LT = getAnalysis<LandmarkTrace>();

	vector<int> thr_ids = LT.get_thr_ids();
	for (size_t k = 0; k < thr_ids.size(); ++k) {

		int i = thr_ids[k];
		size_t n_trunks = LT.get_n_trunks(i);

		InstList prev_landmarks;
		size_t prev_trunk_id = (size_t)-1;

		for (size_t j = 0; j < n_trunks; ++j) {
			if (LT.is_enforcing_landmark(i, j)) {
				unsigned orig_ins_id = LT.get_landmark(i, j).ins_id;
				const InstList &landmarks = CIM.get_instructions(
						i, j, orig_ins_id);
				assert(!landmarks.empty());
				mark_region(prev_landmarks, landmarks, i, prev_trunk_id, j);
				prev_landmarks = landmarks;
				prev_trunk_id = j;
			}
		}

		// Mark the last region. 
		mark_region(prev_landmarks, InstList(), i, prev_trunk_id, (size_t)-1);
	}

	// Check the completeness of region info. 
	forallinst(M, ins) {
		if (!ins_region.count(ins)) {
			BasicBlock *bb = ins->getParent();
			Function *f = bb->getParent();
			if (!(MaxSlicing::is_unreachable(bb) || !MaxSlicing::is_sliced(f)))
				errs() << f->getName() << "." << bb->getName() << ":" << *ins << "\n";
			assert(MaxSlicing::is_unreachable(bb) || !MaxSlicing::is_sliced(f));
		}
	}

	return false;
}

void RegionManager::mark_region(
		const InstList &s_insts, const InstList &e_insts,
		int thr_id, size_t s_tr, size_t e_tr) {

	DEBUG(dbgs() << "mark_region: (" << thr_id << ", " << s_tr <<
			", " << e_tr << ")\n";);
	for (size_t i = 0; i < s_insts.size(); ++i) {
		for (size_t j = i + 1; j < s_insts.size(); ++j)
			assert(s_insts[i]->getParent() != s_insts[j]->getParent());
	}
	for (size_t i = 0; i < e_insts.size(); ++i) {
		for (size_t j = i + 1; j < e_insts.size(); ++j)
			assert(e_insts[i]->getParent() != e_insts[j]->getParent());
	}

	MicroBasicBlockBuilder &MBBB = getAnalysis<MicroBasicBlockBuilder>();
	ICFG &PIB = getAnalysis<PartialICFGBuilder>();
	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
	Reach<ICFGNode> IR;

	DenseSet<const ICFGNode *> visited;
	const ICFGNode *fake_root = PIB[NULL];
	if (s_insts.empty() && e_insts.empty()) {
		
		// Both the start instruction and the end instruction are unknown. 
		// Find any instruction from this thread, and floodfill
		// forwards and backwards. 
		Instruction *ins = CIM.get_any_instruction(thr_id);
		if (!ins) {
			errs() << "[Warning] Cannot find any instruction from thread "
				<< thr_id << "\n";
		} else {
			MicroBasicBlock *mbb = MBBB.parent(ins);
			ICFGNode *node = PIB[mbb];
			assert(node);

			DenseSet<const ICFGNode *> sink, visited_backwards;
			sink.insert(fake_root);
			IR.floodfill_r(node, sink, visited_backwards);
			// We cannot actually reach the fake root. 
			visited_backwards.erase(fake_root);
			visited.insert(visited_backwards.begin(), visited_backwards.end());

			// We need find all nodes connected with <node>. 
			// TODO: Could be much faster if we had an interface to floodfill
			// from multiple nodes. Since this situation is rare, we don't do
			// this currently. 
			forall(DenseSet<const ICFGNode *>, it, visited_backwards) {
				DenseSet<const ICFGNode *> visited_forwards;
				IR.floodfill(*it, DenseSet<const ICFGNode *>(), visited_forwards);
				visited.insert(visited_forwards.begin(), visited_forwards.end());
			}
		} // if (ins)
	} else {

		Reach<ICFGNode> IR;
		DenseSet<const ICFGNode *> src, sink;
		forallconst(InstList, it, s_insts) {
			Instruction *s_ins = *it;
			MicroBasicBlock *m = MBBB.parent(s_ins); assert(m);
			ICFGNode *n = PIB[m]; assert(n);
			src.insert(n);
		}
		forallconst(InstList, it, e_insts) {
			Instruction *e_ins = *it;
			MicroBasicBlock *m = MBBB.parent(e_ins); assert(m);
			ICFGNode *n = PIB[m]; assert(n);
			sink.insert(n);
		}
		assert(!src.empty() || !sink.empty());

		if (!src.empty()) {
			IR.floodfill(src, sink, visited);
		} else {
			IR.floodfill_r(sink, src, visited);
			// We cannot actually reach the fake root. 
			visited.erase(fake_root);
		}
	}

	DEBUG(dbgs() << "# of visited MBBs = " << visited.size() << "\n";);
	assert(!visited.count(fake_root) &&
			"The fake root should have been removed.");
#if 0
	BBList visited_bbs;
	forall(DenseSet<const ICFGNode *>, it, visited) {
		MicroBasicBlock *mbb = (*it)->getMBB();
		assert(mbb);
		visited_bbs.push_back(mbb->getParent());
	}
	sort(visited_bbs.begin(), visited_bbs.end());
	visited_bbs.resize(unique(visited_bbs.begin(), visited_bbs.end()) -
			visited_bbs.begin());
	forall(BBList, it, visited_bbs)
		dbgs() << " " << (*it)->getName();
	dbgs() << "\n";
#endif
	forall(DenseSet<const ICFGNode *>, it, visited) {
		assert(*it);
		const MicroBasicBlock *mbb = (*it)->getMBB();
		assert(mbb);
		BasicBlock::const_iterator s = mbb->begin(), e = mbb->end();
		// <s_insts> are considered to be in this region, but <e_insts> are not. 
		// Instructions in s_insts/e_insts are from different BBs. 
		for (size_t j = 0; j < s_insts.size(); ++j) {
			const Instruction *s_ins = s_insts[j];
			if (mbb == MBBB.parent(s_ins))
				s = s_ins;
		}
		for (size_t j = 0; j < e_insts.size(); ++j) {
			const Instruction *e_ins = e_insts[j];
			if (mbb == MBBB.parent(e_ins))
				e = e_ins;
		}
		for (BasicBlock::const_iterator i = s; i != e; ++i) {
			// Unreachable BBs are added by MaxSlicing. 
			// They don't belong to any region. 
			if (MaxSlicing::is_unreachable(i->getParent()))
				continue;
			Region r;
			r.thr_id = thr_id;
			r.prev_enforcing_landmark = s_tr;
			r.next_enforcing_landmark = e_tr;
			if (ins_region.count(i)) {
				errs() << *i << "\n";
				errs() << "already appeared in " << ins_region[i] << "\n";
			}
			assert(!ins_region.count(i));
			ins_region[i] = r;
		}
	}
}

void RegionManager::get_containing_regions(
		const Instruction *ins, vector<Region> &regions) const {
	regions.clear();
	ConstInstSet visited;
	search_containing_regions(ins, visited, regions);
	sort(regions.begin(), regions.end());
	regions.resize(unique(regions.begin(), regions.end()) - regions.begin());
}

void RegionManager::search_containing_regions(
		const Instruction *ins,
		ConstInstSet &visited,
		vector<Region> &regions) const {
	
	if (visited.count(ins))
		return;
	visited.insert(ins);
	
	/*
	 * If <ins> is in the sliced part, we take the region info right away. 
	 * Otherwise, we trace back to the caller(s) of the containing function. 
	 */
	if (ins_region.count(ins)) {
		regions.push_back(ins_region.lookup(ins));
		return;
	}
	
	// Trace back via the callgraph
	CallGraphFP &CG = getAnalysis<CallGraphFP>();
	const Function *f = ins->getParent()->getParent();
	InstList call_sites = CG.get_call_sites(f);
	forall(InstList, it, call_sites)
		search_containing_regions(*it, visited, regions);
}

void RegionManager::print(raw_ostream &O, const Module *M) const {
}
