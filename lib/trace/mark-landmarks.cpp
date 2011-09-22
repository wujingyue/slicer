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

#include "slicer/enforcing-landmarks.h"
#include "slicer/mark-landmarks.h"
using namespace slicer;

static RegisterPass<MarkLandmarks> X("mark-landmarks",
		"Mark landmarks",
		false, true); // is analysis

static cl::opt<bool> DisableDerivedLandmarks("disable-derived-landmarks",
		cl::desc("Don't mark any derived landmarks"));

STATISTIC(NumEnforcingLandmarks, "Number of enforcing landmarks");
STATISTIC(NumDerivedLandmarks, "Number of derived landmarks");

char MarkLandmarks::ID = 0;

bool MarkLandmarks::runOnModule(Module &M) {
	EnforcingLandmarks &EL = getAnalysis<EnforcingLandmarks>();
	Exec &EXE = getAnalysis<Exec>();
	InstSet enf_landmarks = EL.get_enforcing_landmarks();
	ConstInstSet const_enf_landmarks;
	for (InstSet::iterator itr = enf_landmarks.begin();
			itr != enf_landmarks.end(); ++itr) {
		const_enf_landmarks.insert(*itr);
	}
	EXE.setup_landmarks(const_enf_landmarks);
	EXE.run();

	landmarks.clear();
	mark_enforcing_landmarks(M);
	mark_thread_exits(M);
	if (!DisableDerivedLandmarks) {
		mark_enforcing_recursive_returns(M);
		mark_enforcing_calls(M);
	}

	NumEnforcingLandmarks = enf_landmarks.size();
	NumDerivedLandmarks = landmarks.size() - enf_landmarks.size();

	return false;
}

void MarkLandmarks::mark_enforcing_calls(Module &M) {
	Exec &EXE = getAnalysis<Exec>();

	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				if (is_call(ins) && EXE.may_exec_landmark(ins))
					landmarks.insert(ins);
			}
		}
	}
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

void MarkLandmarks::mark_enforcing_recursive_returns(Module &M) {
	CallGraph &raw_CG = getAnalysis<CallGraphFP>();
	CallGraphFP &CG = getAnalysis<CallGraphFP>();
	Exec &EXE = getAnalysis<Exec>();

	FuncSet enf_recursive_funcs;
	for (scc_iterator<CallGraph *> si = scc_begin(&raw_CG), E = scc_end(&raw_CG);
			si != E; ++si) {
		if (si.hasLoop()) {
			for (size_t i = 0; i < (*si).size(); ++i) {
				Function *f = (*si)[i]->getFunction();
				if (f && EXE.may_exec_landmark(f))
					enf_recursive_funcs.insert(f);
			}
		}
	}

	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				if (is_call(ins)) {
					bool calls_enf_recursive_funcs = false;
					FuncList callees = CG.get_called_functions(ins);
					for (size_t i = 0; i < callees.size(); ++i) {
						if (enf_recursive_funcs.count(callees[i])) {
							calls_enf_recursive_funcs = true;
							break;
						}
					}
					if (calls_enf_recursive_funcs) {
						if (isa<CallInst>(ins)) {
							BasicBlock::iterator next = ins; ++next;
							landmarks.insert(next);
						} else {
							assert(isa<InvokeInst>(ins));
							InvokeInst *ii = cast<InvokeInst>(ins);
							// FIXME: Assume we never goes to the exception block. 
							landmarks.insert(ii->getNormalDest()->begin());
						}
					}
				}
			}
		}
	}
}

void MarkLandmarks::mark_returns(Function *f) {
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

void MarkLandmarks::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequiredTransitive<IDManager>();
	AU.addRequired<EnforcingLandmarks>();
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
