/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "trace"

#include "llvm/Support/CFG.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/Statistic.h"
#include "common/identify-thread-funcs.h"
#include "common/exec.h"
#include "common/callgraph-fp.h"
#include "common/IDManager.h"
using namespace llvm;

#include "enforcing-landmarks.h"
#include "mark-landmarks.h"
#include "omit-branch.h"
using namespace slicer;

static RegisterPass<MarkLandmarks> X("mark-landmarks",
		"Mark landmarks",
		false, true); // is analysis

static cl::opt<bool> DisableDerivedLandmarks("disable-derived-landmarks",
		cl::desc("Don't mark any derived landmarks"));

STATISTIC(NumOmittedBranches, "Number of omitted branches");
STATISTIC(NumRemainingBranches, "Number of remaining branches");

char MarkLandmarks::ID = 0;

bool MarkLandmarks::runOnModule(Module &M) {
	landmarks.clear();
	mark_enforcing_landmarks(M);
	mark_thread_exits(M);
	if (!DisableDerivedLandmarks) {
		mark_branch_succs(M);
		mark_recursive_rets(M);
	}
	return false;
}

void MarkLandmarks::mark_thread_exits(Module &M) {
	IdentifyThreadFuncs &ITF = getAnalysis<IdentifyThreadFuncs>();
	
	forallfunc(M, fi) {
		if (fi->isDeclaration())
			continue;
		if (ITF.is_thread_func(fi) || is_main(fi)) {
			// This should be a pthread_self() call added by the preparer at
			// each thread function entry. 
			// No harm marking it again. 
			landmarks.insert(fi->begin()->getFirstNonPHI());
			// Theoretically, there's a pthread_self() call at each exit, but
			// real exits are returns or pthread_exit() calls. 
			forall(Function, bi, *fi) {
				if (succ_begin(bi) == succ_end(bi))
					landmarks.insert(bi->getTerminator());
			}
		}
	}
}

void MarkLandmarks::mark_recursive_rets(Module &M) {
	EnforcingLandmarks &EL = getAnalysis<EnforcingLandmarks>();
	Exec &EXE = getAnalysis<Exec>();
	CallGraph &CG = getAnalysis<CallGraphFP>();

	InstSet landmarks = EL.get_enforcing_landmarks();
	ConstInstSet const_landmarks;
	for (InstSet::iterator itr = landmarks.begin(); itr != landmarks.end();
			++itr) {
		const_landmarks.insert(*itr);
	}
	EXE.setup_landmarks(const_landmarks);
	EXE.run();

	for (scc_iterator<CallGraph *> si = scc_begin(&CG), E = scc_end(&CG);
			si != E; ++si) {
		if (si.hasLoop()) {
			for (size_t i = 0; i < (*si).size(); ++i) {
				Function *f = (*si)[i]->getFunction();
				if (f && EXE.may_exec_landmark(f))
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
		TerminatorInst *ti = bb->getTerminator();
		if (ti->getNumSuccessors() >= 2) {
			if (OB.omit(ti)) {
				++NumOmittedBranches;
			} else {
				++NumRemainingBranches;
				for (unsigned i = 0; i < ti->getNumSuccessors(); ++i) {
					BasicBlock *succ = ti->getSuccessor(i);
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
	AU.addRequired<Exec>();
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
