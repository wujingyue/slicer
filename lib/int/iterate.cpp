/**
 * Author: Jingyue
 */

#include "bc2bdd/BddAliasAnalysis.h"
using namespace bc2bdd;

#include "llvm/LLVMContext.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/Debug.h"
#include "slicer/InitializePasses.h"
using namespace llvm;

#include "slicer/capture.h"
#include "slicer/solve.h"
#include "slicer/adv-alias.h"
#include "slicer/iterate.h"
#include "slicer/stratify-loads.h"
using namespace slicer;

#include <sstream>
using namespace std;

INITIALIZE_PASS_BEGIN(Iterate, "iterate",
		"A iterator to provide more accurate analyses", false, true)
INITIALIZE_PASS_DEPENDENCY(StratifyLoads)
INITIALIZE_PASS_DEPENDENCY(CaptureConstraints)
INITIALIZE_PASS_DEPENDENCY(SolveConstraints)
INITIALIZE_PASS_DEPENDENCY(AdvancedAlias)
INITIALIZE_PASS_END(Iterate, "iterate",
		"A iterator to provide more accurate analyses", false, true)

void Iterate::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<StratifyLoads>();
	AU.addRequired<CaptureConstraints>();
	AU.addRequired<SolveConstraints>();
	AU.addRequired<AdvancedAlias>();
}

Iterate::Iterate(): ModulePass(ID) {
	initializeIteratePass(*PassRegistry::getPassRegistry());
}

char Iterate::ID = 0;

bool Iterate::runOnModule(Module &M) {
	StratifyLoads &SL = getAnalysis<StratifyLoads>();
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	AdvancedAlias &AAA = getAnalysis<AdvancedAlias>();
	/*
	 * # of constraints is not a good indicator to decide whether we can
	 * stop the iterating process. 
	 * It may increase, decrease or unchange after an iteration. 
	 */
	TimerGroup tg("Iterator");
	vector<Timer *> timers;

	unsigned max_level = SL.get_max_level();
	errs() << "max_level = " << max_level << "\n";
	for (unsigned iter_no = 1; ; ++iter_no) {
		ostringstream oss; oss << "Iteration " << iter_no;
		Timer *timer = new Timer(oss.str(), tg);
		timers.push_back(timer);
		timer->startTimer();
		dbgs() << "=== Iterator is running iteration " << iter_no << "... ===\n";
		long fingerprint = CC.get_fingerprint();
		AAA.recalculate(M); // Essentially clear the cache. 
		CC.set_current_level(iter_no);
		CC.recalculate(M);
		timer->stopTimer();
		AAA.print(dbgs(), &M); // Print stats in AAA. 
		errs() << "Old fingerprint = " << fingerprint << "\n";
		errs() << "New fingerprint = " << CC.get_fingerprint() << "\n";
		if (CC.get_fingerprint() == fingerprint && iter_no >= max_level)
			break;
		SC.recalculate(M);
	}

	for (size_t i = 0; i < timers.size(); ++i)
		delete timers[i];

	return false;
}
