/**
 * Author: Jingyue
 */

#include "llvm/Support/Debug.h"
#include "common/cfg/partial-icfg-builder.h"
#include "common/cfg/reach.h"
#include "idm/mbb.h"
using namespace llvm;

#include "region-manager.h"
#include "clone-info-manager.h"
#include "../trace/landmark-trace.h"
using namespace slicer;

static RegisterPass<RegionManager> X(
		"manage-region",
		"Mark each cloned instruction with its previous enforcing landmark "
		"and next enforcing landmark",
		false,
		true);

char RegionManager::ID = 0;

void RegionManager::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesCFG();
	AU.addRequired<CloneInfoManager>();
	AU.addRequired<LandmarkTrace>();
	AU.addRequired<MicroBasicBlockBuilder>();
	AU.addRequired<PartialICFGBuilder>();
	ModulePass::getAnalysisUsage(AU);
}

bool RegionManager::runOnModule(Module &M) {

	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
	LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
	
	vector<int> thr_ids = LT.get_thr_ids();
	for (size_t k = 0; k < thr_ids.size(); ++k) {

		int i = thr_ids[k];
		size_t n_trunks = LT.get_n_trunks(i);

		Instruction *prev_enforcing_landmark = NULL;
		size_t prev_enforcing_trunk_id = (size_t)-1;

		for (size_t j = 0; j < n_trunks; ++j) {
			if (LT.is_enforcing_landmark(i, j)) {
				unsigned orig_ins_id = LT.get_landmark(i, j).ins_id;
				Instruction *enforcing_landmark = CIM.get_instruction(
						i, j, orig_ins_id);
				if (!enforcing_landmark) {
					errs() << "(" << i << ", " << j << ", " <<
						orig_ins_id << ") not found\n";
					assert(false && "Some enforcing landmarks cannot be found.");
				}
				mark_region(
						prev_enforcing_landmark, enforcing_landmark,
						i, prev_enforcing_trunk_id, j);
				prev_enforcing_landmark = enforcing_landmark;
				prev_enforcing_trunk_id = j;
			}
		}

		mark_region(
				prev_enforcing_landmark, NULL, i, prev_enforcing_trunk_id, (size_t)-1);
	}
	return true;
}

void RegionManager::mark_region(
		const Instruction *s_ins, const Instruction *e_ins,
		int thr_id, size_t s_tr, size_t e_tr) {

	DEBUG(dbgs() << "mark_region: thr_id = " << thr_id << "\n";
	if (!s_ins)
		dbgs() << "  <null>\n";
	else
		dbgs() << *s_ins << "\n";
	if (!e_ins)
		dbgs() << "  <null>\n";
	else
		dbgs() << *e_ins << "\n";);

	MicroBasicBlockBuilder &MBBB = getAnalysis<MicroBasicBlockBuilder>();
	ICFG &PIB = getAnalysis<PartialICFGBuilder>();
	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
	Reach<ICFGNode> IR;

	DenseSet<const ICFGNode *> visited;
	const ICFGNode *fake_root = PIB[NULL];
	if (s_ins == NULL && e_ins == NULL) {
		
		// Both the start instruction and the end instruction are unknown. 
		// Find any instruction from this thread, and floodfill
		// forwards and backwards. 
		Instruction *ins = CIM.get_any_instruction(thr_id);
		MicroBasicBlock *mbb = MBBB.parent(ins);
		ICFGNode *node = PIB[mbb];
		assert(node);

		DenseSet<const ICFGNode *> sink, visited_backwards;
		sink.insert(fake_root);
		IR.floodfill_r(node, sink, visited_backwards);
		visited.insert(visited_backwards.begin(), visited_backwards.end());
		// We cannot actually reach the fake root. 
		visited.erase(fake_root);
		
		// We need find all nodes connected with <node>. 
		// TODO: Could be much faster if we had an interface to floodfill
		// from multiple nodes. Since this situation is rare, we don't do
		// this currently. 
		forall(DenseSet<const ICFGNode *>, it, visited_backwards) {
			DenseSet<const ICFGNode *> visited_forwards;
			IR.floodfill(*it, DenseSet<const ICFGNode *>(), visited_forwards);
			visited.insert(visited_forwards.begin(), visited_forwards.end());
		}
	} else {

		MicroBasicBlock *m1 = MBBB.parent(s_ins), *m2 = MBBB.parent(e_ins);
		ICFGNode *n1 = (m1 == NULL ? NULL : PIB[m1]);
		ICFGNode *n2 = (m2 == NULL ? NULL : PIB[m2]);
		if (m1) assert(n1);
		if (m2) assert(n2);
		assert(n1 || n2);

		Reach<ICFGNode> IR;
		DenseSet<const ICFGNode *> sink;
		if (n1 && n2) {
			sink.insert(n2);
			IR.floodfill(n1, sink, visited);
		} else if (n1) {
			IR.floodfill(n1, DenseSet<const ICFGNode *>(), visited);
		} else {
			sink.insert(fake_root);
			IR.floodfill_r(n2, sink, visited);
			// We cannot actually reach the fake root. 
			visited.erase(fake_root);
		}
	}

	dbgs() << "# of visited MBBs = " << visited.size() << "\n";
	assert(!visited.count(fake_root) && "The fake root should be removed");
	forall(DenseSet<const ICFGNode *>, it, visited) {
		assert(*it);
		const MicroBasicBlock *mbb = (*it)->getMBB();
		assert(mbb);
		BasicBlock::const_iterator s = mbb->begin(), e = mbb->end();
		// <s_ins> is considered to be in this region, but <e_ins> is not. 
		if (mbb == MBBB.parent(s_ins))
			s = s_ins;
		if (mbb == MBBB.parent(e_ins))
			e = e_ins;
		for (BasicBlock::const_iterator i = s; i != e; ++i) {
			Region r;
			r.thr_id = thr_id;
			r.prev_enforcing_landmark = s_tr;
			r.next_enforcing_landmark = e_tr;
			ins_region[i] = r;
		}
	}
}

Region RegionManager::get_region(const Instruction *ins) const {
	return ins_region.lookup(ins);
}
