#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "llvm/LLVMContext.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

#include "capture.h"
#include "solve.h"
#include "adv-alias.h"
#include "iterate.h"
using namespace slicer;

#include <sstream>
using namespace std;

static RegisterPass<Iterate> X(
		"iterate",
		"A iterator to provide more accurate analyses",
		false,
		false); // Not an analysis.

char Iterate::ID = 0;

bool Iterate::runOnModule(Module &M) {

	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	AdvancedAlias &AAA = getAnalysis<AdvancedAlias>();
	CC.replace_aa(&AAA);
	/*
	 * # of constraints is not a good indicator to decide whether we can
	 * stop the iterating process. 
	 * It may increase, decrease or unchange after an iteration. 
	 */
	long fingerprint;
	TimerGroup tg("Iterator");
	vector<Timer *> timers;
	int iter_no = 0;
	do {
		++iter_no;
		ostringstream oss; oss << "Iteration " << iter_no;
		Timer *timer = new Timer(oss.str(), tg);
		timers.push_back(timer);
		timer->startTimer();
		dbgs() << "=== Running iteration " << iter_no << "... ===\n";
		fingerprint = CC.get_fingerprint();
		AAA.recalculate(M); // Essentially clear the cache. 
		CC.recalculate(M);
		SC.recalculate(M);
		timer->stopTimer();
	} while (CC.get_fingerprint() != fingerprint);
	for (size_t i = 0; i < timers.size(); ++i)
		delete timers[i];

	return false;
}

void Iterate::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<ObjectID>();
	AU.addRequired<CaptureConstraints>();
	AU.addRequired<SolveConstraints>();
	AU.addRequired<AdvancedAlias>();
	ModulePass::getAnalysisUsage(AU);
}
