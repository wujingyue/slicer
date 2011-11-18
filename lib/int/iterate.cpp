#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

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
using namespace slicer;

#include <sstream>
using namespace std;

INITIALIZE_PASS_BEGIN(Iterate, "iterate",
		"A iterator to provide more accurate analyses", false, true)
INITIALIZE_PASS_DEPENDENCY(CaptureConstraints)
INITIALIZE_PASS_DEPENDENCY(SolveConstraints)
INITIALIZE_PASS_DEPENDENCY(AdvancedAlias)
INITIALIZE_PASS_END(Iterate, "iterate",
		"A iterator to provide more accurate analyses", false, true)

void Iterate::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<CaptureConstraints>();
	AU.addRequired<SolveConstraints>();
	AU.addRequired<AdvancedAlias>();
}

char Iterate::ID = 0;

bool Iterate::runOnModule(Module &M) {
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	AdvancedAlias &AAA = getAnalysis<AdvancedAlias>();
	/*
	 * # of constraints is not a good indicator to decide whether we can
	 * stop the iterating process. 
	 * It may increase, decrease or unchange after an iteration. 
	 */
	long fingerprint;
	TimerGroup tg("Iterator");
	vector<Timer *> timers;
	for (int iter_no = 1; ; ++iter_no) {
		ostringstream oss; oss << "Iteration " << iter_no;
		Timer *timer = new Timer(oss.str(), tg);
		timers.push_back(timer);
		timer->startTimer();
		dbgs() << "=== Iterator is running iteration " << iter_no << "... ===\n";
		fingerprint = CC.get_fingerprint();
		AAA.recalculate(M); // Essentially clear the cache. 
		CC.recalculate(M);
		timer->stopTimer();
		AAA.print(dbgs(), &M); // Print stats in AAA. 
		if (CC.get_fingerprint() == fingerprint)
			break;
		SC.recalculate(M);
	}

	for (size_t i = 0; i < timers.size(); ++i)
		delete timers[i];

	return false;
}
