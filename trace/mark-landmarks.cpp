#include "llvm/Support/CFG.h"
#include "common/cfg/identify-thread-funcs.h"
#include "common/id-manager/IDManager.h"
#include "common/include/util.h"
using namespace llvm;

#include "enforcing-landmarks.h"
#include "mark-landmarks.h"
#include "omit-branch.h"
using namespace slicer;

static RegisterPass<MarkLandmarks> X(
		"mark-landmarks",
		"Mark landmarks",
		false,
		true); // is analysis

char MarkLandmarks::ID = 0;

bool MarkLandmarks::runOnModule(Module &M) {

	landmarks.clear();
	mark_enforcing_landmarks(M);
	mark_branch_succs(M);
	mark_thread(M);
	return false;
}

void MarkLandmarks::mark_enforcing_landmarks(Module &M) {

	EnforcingLandmarks &EL = getAnalysis<EnforcingLandmarks>();
	const InstSet &enforcing_landmarks = EL.get_enforcing_landmarks();
	landmarks.insert(enforcing_landmarks.begin(), enforcing_landmarks.end());
}

void MarkLandmarks::mark_thread(Module &M) {
	IdentifyThreadFuncs &ITF = getAnalysis<IdentifyThreadFuncs>();
	forallfunc(M, fi) {
		if (fi->isDeclaration())
			continue;
		if (ITF.is_thread_func(fi) || is_main(fi)) {
			Instruction *first = fi->getEntryBlock().getFirstNonPHI();
			landmarks.insert(first);
			forall(Function, bi, *fi) {
				if (succ_begin(bi) == succ_end(bi))
					landmarks.insert(bi->getTerminator());
			}
		}
	}
}

void MarkLandmarks::mark_branch_succs(Module &M) {
	OmitBranch &OB = getAnalysis<OmitBranch>();
	forallinst(M, ii) {
		BranchInst *bi = dyn_cast<BranchInst>(ii);
		if (bi && !OB.omit(bi)) {
			for (unsigned i = 0; i < bi->getNumSuccessors(); ++i) {
				BasicBlock *succ = bi->getSuccessor(i);
				landmarks.insert(succ->getFirstNonPHI());
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
