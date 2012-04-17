/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "int"

#include "llvm/Support/Debug.h"
#include "bc2bdd/InitializePasses.h"
#include "common/InitializePasses.h"
#include "slicer/InitializePasses.h"
using namespace llvm;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace bc2bdd;

#include "common/util.h"
#include "common/callgraph-fp.h"
#include "common/exec-once.h"
using namespace rcs;

#include "slicer/may-write-analyzer.h"
#include "slicer/adv-alias.h"
using namespace slicer;

INITIALIZE_PASS_BEGIN(MayWriteAnalyzer, "analyze-may-write",
		"Analyze may write behaviors", false, true)
INITIALIZE_PASS_DEPENDENCY(BddAliasAnalysis)
INITIALIZE_PASS_DEPENDENCY(CallGraphFP)
INITIALIZE_PASS_DEPENDENCY(ExecOnce)
INITIALIZE_PASS_END(MayWriteAnalyzer, "analyze-may-write",
		"Analyze may write behaviors", false, true)

void MayWriteAnalyzer::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequiredTransitive<BddAliasAnalysis>();
	AU.addRequiredTransitive<CallGraphFP>();
	AU.addRequiredTransitive<ExecOnce>();
}

char MayWriteAnalyzer::ID = 0;

MayWriteAnalyzer::MayWriteAnalyzer(): ModulePass(ID) {
	initializeMayWriteAnalyzerPass(*PassRegistry::getPassRegistry());
}

bool MayWriteAnalyzer::runOnModule(Module &M) {
	return false;
}

bool MayWriteAnalyzer::may_write(const Instruction *i,
		const Value *q, ConstFuncSet &visited_funcs, bool trace_callee) {
	if (const StoreInst *si = dyn_cast<StoreInst>(i)) {
		if (may_alias(si->getPointerOperand(), q)) {
			DEBUG(dbgs() << "may_write: ";);
			DEBUG(dbgs() << "[" << si->getParent()->getParent()->getName() << "]";);
			DEBUG(dbgs() << *si << "\n";);
			return true;
		}
	}

	CallSite cs(const_cast<Instruction *>(i));
	if (cs.getInstruction() && !is_pthread_create(i)) {
		CallGraphFP &CG = getAnalysis<CallGraphFP>();
		FuncList callees = CG.get_called_functions(i);
		for (size_t j = 0; j < callees.size(); ++j) {
			Function *callee = callees[j];
			if (callee->isDeclaration()) {
				if (libcall_may_write(cs, q)) {
					DEBUG(dbgs() << "may_write:" << *i << "\n";);
					return true;
				}
			} else {
				// <callee> is an internal function. 
				if (trace_callee) {
					ExecOnce &EO = getAnalysis<ExecOnce>();
					// Don't trace into exec-once functions, because they are already
					// included in the partical ICFG and potentially included in the path. 
					if (!EO.not_executed(callee) && !EO.executed_once(callee)) {
						if (may_write(callee, q, visited_funcs))
							return true;
					}
				}
			}
		}
	}

	return false;
}

bool MayWriteAnalyzer::libcall_may_write(const CallSite &cs, const Value *q) {
	assert(cs.getInstruction());

	if (Function *callee = cs.getCalledFunction()) {
		if (callee->getName() == "fscanf") {
			assert(cs.arg_size() >= 2);
			for (unsigned arg_no = 2; arg_no < cs.arg_size(); ++arg_no) {
				if (may_alias(cs.getArgument(arg_no), q))
					return true;
			}
		}
		if (callee->getName() == "BZ2_bzReadOpen" ||
				callee->getName() == "BZ2_bzRead" ||
				callee->getName() == "BZ2_bzReadGetUnused" ||
				callee->getName() == "BZ2_bzReadClose") {
			assert(cs.arg_size() >= 1);
			if (may_alias(cs.getArgument(0), q))
				return true;
		}
		if (callee->getName().find("isoc99_scanf") != string::npos) {
			assert(cs.arg_size() >= 1);
			for (unsigned arg_no = 1; arg_no < cs.arg_size(); ++arg_no) {
				if (may_alias(q, cs.getArgument(arg_no)))
					return true;
			}
		}
	}

	return false;
}

bool MayWriteAnalyzer::may_write(const Function *f,
		const Value *q, ConstFuncSet &visited_funcs, bool trace_callee) {
	if (visited_funcs.count(f))
		return false;
	visited_funcs.insert(f);

	if (f->isDeclaration())
		return false;
	
	for (Function::const_iterator bi = f->begin(); bi != f->end(); ++bi) {
		for (BasicBlock::const_iterator ii = bi->begin(); ii != bi->end(); ++ii) {
			if (may_write(ii, q, visited_funcs, trace_callee))
				return true;
		}
	}
	
	return false;
}

bool MayWriteAnalyzer::may_alias(const Value *v1, const Value *v2) {
	AdvancedAlias *AAA = getAnalysisIfAvailable<AdvancedAlias>();
	if (AAA) {
		return AAA->may_alias(v1, v2);
	} else {
		AliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
		return BAA.alias(v1, 0, v2, 0) == AliasAnalysis::MayAlias;
	}
}

