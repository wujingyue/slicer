/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "trace"

#include "llvm/Support/CFG.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/Statistic.h"
#include "common/cfg/identify-thread-funcs.h"
#include "common/cfg/may-exec.h"
#include "common/callgraph-fp/callgraph-fp.h"
#include "common/id-manager/IDManager.h"
using namespace llvm;

#include "enforcing-landmarks.h"
#include "mark-landmarks.h"
#include "omit-branch.h"
using namespace slicer;

static RegisterPass<MarkLandmarks> X("mark-landmarks",
		"Mark landmarks",
		false, true); // is analysis

STATISTIC(NumOmittedBranches, "Number of omitted branches");
STATISTIC(NumRemainingBranches, "Number of remaining branches");

char MarkLandmarks::ID = 0;

bool MarkLandmarks::runOnModule(Module &M) {
	landmarks.clear();
	mark_enforcing_landmarks(M);
	mark_branch_succs(M);
	mark_thread_exits(M);
	mark_recursive_rets(M);
	return false;
}

void MarkLandmarks::mark_thread_exits(Module &M) {
	IdentifyThreadFuncs &ITF = getAnalysis<IdentifyThreadFuncs>();
	
	forallfunc(M, fi) {
		if (fi->isDeclaration())
			continue;
		if (ITF.is_thread_func(fi) || is_main(fi)) {
			landmarks.insert(fi->begin()->getFirstNonPHI());
			forall(Function, bi, *fi) {
				if (succ_begin(bi) == succ_end(bi))
					landmarks.insert(bi->getTerminator());
			}
		}
	}
}

void MarkLandmarks::mark_recursive_rets(Module &M) {
	
	EnforcingLandmarks &EL = getAnalysis<EnforcingLandmarks>();
	MayExec &ME = getAnalysis<MayExec>();
	CallGraph &CG = getAnalysis<CallGraphFP>();

	ME.setup_landmarks(EL.get_enforcing_landmarks());
	ME.run();

	for (scc_iterator<CallGraph *> si = scc_begin(&CG), E = scc_end(&CG);
			si != E; ++si) {
		if (si.hasLoop()) {
			for (size_t i = 0; i < (*si).size(); ++i) {
				Function *f = (*si)[i]->getFunction();
				if (f && ME.may_exec_landmark(f))
					mark_rets(f);
			}
		}
	}
}

void MarkLandmarks::mark_rets(Function *f) {
	forall(Function, bb, *f) {
		TerminatorInst *ti = bb->getTerminator();
		if (is_ret(ti))
			landmarks.insert(ti);
	}
}

void MarkLandmarks::mark_enforcing_landmarks(Module &M) {
	EnforcingLandmarks &EL = getAnalysis<EnforcingLandmarks>();
	const InstSet &enforcing_landmarks = EL.get_enforcing_landmarks();
	landmarks.insert(enforcing_landmarks.begin(), enforcing_landmarks.end());
}

void MarkLandmarks::mark_branch_succs(Module &M) {
	OmitBranch &OB = getAnalysis<OmitBranch>();
	forallbb(M, bb) {
		BranchInst *bi = dyn_cast<BranchInst>(bb->getTerminator());
		if (bi) {
			if (OB.omit(bi)) {
				++NumOmittedBranches;
			} else {
				++NumRemainingBranches;
				for (unsigned i = 0; i < bi->getNumSuccessors(); ++i) {
					BasicBlock *succ = bi->getSuccessor(i);
					landmarks.insert(succ->getFirstNonPHI());
				}
			}
		}
	}
}

void MarkLandmarks::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequiredTransitive<IDManager>();
	AU.addRequired<EnforcingLandmarks>();
	AU.addRequired<OmitBranch>();
	AU.addRequired<IdentifyThreadFuncs>();
	AU.addRequired<MayExec>();
	AU.addRequired<CallGraphFP>();
	ModulePass::getAnalysisUsage(AU);
}

void MarkLandmarks::print(raw_ostream &O, const Module *M) const {
	IDManager &IDM = getAnalysis<IDManager>();
	vector<unsigned> all_inst_ids;
	forallconst(InstSet, it, landmarks) {
		unsigned ins_id = IDM.getInstructionID(*it);
		assert(ins_id != IDManager::INVALID_ID);
		all_inst_ids.push_back(ins_id);
	}
	sort(all_inst_ids.begin(), all_inst_ids.end());
	for (size_t i = 0; i < all_inst_ids.size(); ++i)
		O << all_inst_ids[i] << "\n";
}

const InstSet &MarkLandmarks::get_landmarks() const {
	return landmarks;
}
