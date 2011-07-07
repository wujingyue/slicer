/**
 * Author: Jingyue
 */

#include "llvm/Module.h"
#include "llvm/Analysis/PostDominators.h"
#include "common/callgraph-fp/callgraph-fp.h"
#include "common/cfg/may-exec.h"
#include "idm/id.h"
using namespace llvm;

#include <set>
#include <map>
#include <fstream>
using namespace std;

#include "omit-branch.h"
#include "enforcing-landmarks.h"
using namespace slicer;

void OmitBranch::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequiredTransitive<CallGraphFP>();
	AU.addRequiredTransitive<MayExec>();
	AU.addRequired<EnforcingLandmarks>();
	AU.addRequiredTransitive<PostDominatorTree>();
	ModulePass::getAnalysisUsage(AU);
}

bool OmitBranch::runOnModule(Module &M) {
	MayExec &ME = getAnalysis<MayExec>();
	EnforcingLandmarks &EL = getAnalysis<EnforcingLandmarks>();
	ME.setup_landmarks(EL.get_enforcing_landmarks());
	ME.run();
	return false;
}

void OmitBranch::print(raw_ostream &O, const Module *M) const {
}

void OmitBranch::dfs(BasicBlock *x, BasicBlock *sink) {
	if (visited[x])
		return;
	// Stop at the sink -- the post dominator of the branch
	if (x == sink)
		return;
	visited[x] = true;
	for (succ_iterator it = succ_begin(x); it != succ_end(x); ++it) {
		BasicBlock *y = *it;
		dfs(y, sink);
	}
}

/* Copied from LLVM SVN */
BasicBlock *find_nearest_common_post_dominator(
		PostDominatorTree &PDT, BasicBlock *A, BasicBlock *B) {
	// If B dominates A then B is nearest common dominator.
	if (PDT.dominates(B, A))
		return B;

	// If A dominates B then A is nearest common dominator.
	if (PDT.dominates(A, B))
		return A;

	DomTreeNodeBase<BasicBlock> *NodeA = PDT.getNode(A);
	DomTreeNodeBase<BasicBlock> *NodeB = PDT.getNode(B);

	// Collect NodeA dominators set.
	SmallPtrSet<DomTreeNodeBase<BasicBlock>*, 16> NodeADoms;
	NodeADoms.insert(NodeA);
	DomTreeNodeBase<BasicBlock> *IDomA = NodeA->getIDom();
	while (IDomA) {
		NodeADoms.insert(IDomA);
		IDomA = IDomA->getIDom();
	}

	// Walk NodeB immediate dominators chain and find common dominator node.
	DomTreeNodeBase<BasicBlock> *IDomB = NodeB->getIDom();
	while (IDomB) {
		if (NodeADoms.count(IDomB) != 0)
			return IDomB->getBlock();

		IDomB = IDomB->getIDom();
	}

	return NULL;
}

bool OmitBranch::omit(BranchInst *branch) {

	BasicBlock *bb = branch->getParent();
	assert(branch->getNumSuccessors() > 0 && "The branch has no successor.");
	Function *func = bb->getParent();
	/* Calculate the nearest post dominator of <bb> */
	PostDominatorTree &PDT = getAnalysis<PostDominatorTree>(*func);
	BasicBlock *post_dominator_bb = branch->getSuccessor(0);
	/*
	 * If <bb> has only one successor, <post_dominator_bb> will be that
	 * successor, and then <dfs> won't visit any BB. 
	 */
	for (unsigned i = 1; i < branch->getNumSuccessors(); i++) {
		/*
		 * findNearestCommonDominator does not work with a post dominator tree
		 * in LLVM 2.7 release. 
		 */
#if 0
		post_dominator_bb = PDT.findNearestCommonDominator(
				post_dominator_bb,
				branch->getSuccessor(i));
#endif
		post_dominator_bb = find_nearest_common_post_dominator(
				PDT,
				post_dominator_bb,
				branch->getSuccessor(i));
		if (!post_dominator_bb)
			break;
	}
	/* Flood fill from <bb> until reaching <post_dominator_bb> */
	visited.clear();
	for (Function::iterator bi = func->begin(); bi != func->end(); ++bi)
		visited[bi] = false;
	for (unsigned i = 0; i < branch->getNumSuccessors(); i++)
		dfs(branch->getSuccessor(i), post_dominator_bb);
	/* If any visited BB has a sync operation, the branch cannot be omitted. */
	for (Function::iterator bi = func->begin(); bi != func->end(); ++bi) {
		if (!visited[bi])
			continue;
		MayExec &ME = getAnalysis<MayExec>();
		for (BasicBlock::iterator ii = bi->begin(); ii != bi->end(); ++ii) {
			if (ME.is_landmark(ii))
				return false;
			if (is_call(ii) && !is_intrinsic_call(ii)) {
				CallGraphFP &CG = getAnalysis<CallGraphFP>();
				const FuncList &called_funcs =
					CG.get_called_functions(ii);
				for (size_t i = 0; i < called_funcs.size(); ++i) {
					if (ME.may_exec_landmark(called_funcs[i]))
						return false;
				}
			}
		}
	}
	return true;
}

char OmitBranch::ID = 0;

static RegisterPass<slicer::OmitBranch> X(
		"omit-branch",
		"Determine whether a branch affects the trace of sync event",
		false,
		true); // is analysis
