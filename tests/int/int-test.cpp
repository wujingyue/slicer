/**
 * Author: Jingyue
 */

/**
 * Identify the variables only? 
 * Saves the solving time during debugging. 
 */
// #define IDENTIFY_ONLY

#include <map>
#include <vector>
using namespace std;

#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/DominatorInternals.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "common/util.h"
#include "common/exec-once.h"
using namespace llvm;

#include "slicer/iterate.h"
#include "slicer/capture.h"
#include "slicer/adv-alias.h"
#include "slicer/solve.h"
#include "slicer/test-utils.h"
#include "slicer/region-manager.h"
#include "int-test.h"
using namespace slicer;

static RegisterPass<IntTest> X("int-test",
		"Test the integer constraint solver");

// If Program == "", we dump all the integer constraints. 
static cl::opt<string> Program("prog",
		cl::desc("The program being tested (e.g. aget). "
			"Usually it's simply the name of the bc file without \".bc\"."));

char IntTest::ID = 0;

void IntTest::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<ExecOnce>();
	AU.addRequired<RegionManager>();
	AU.addRequired<IDAssigner>();
#ifndef IDENTIFY_ONLY
	AU.addRequired<Iterate>();
	AU.addRequired<SolveConstraints>();
	AU.addRequired<CaptureConstraints>();
	AU.addRequired<AdvancedAlias>();
#endif
	ModulePass::getAnalysisUsage(AU);
}

bool IntTest::runOnModule(Module &M) {
	if (Program == "") {
		CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
		CC.print(errs(), &M);
#if 0
		SolveConstraints &SC = getAnalysis<SolveConstraints>();
		SC.print_assertions();
#endif
		return false;
	}
	/**
	 * Run all test cases. 
	 * Each test case will check whether it should be run according
	 * to the program name. 
	 */
	if (Program == "aget")
		test_aget(M);
	if (Program == "aget-like")
		test_aget_like(M);
	if (Program == "test-overwrite")
		test_test_overwrite(M);
	if (Program == "test-overwrite-2")
		test_test_overwrite_2(M);
	if (Program == "FFT")
		test_fft(M);
	if (Program == "FFT-like")
		test_fft_like(M);
	if (Program == "FFT-tern")
		test_fft_tern(M);
	if (Program == "RADIX")
		test_radix(M);
	if (Program == "RADIX-like")
		test_radix_like(M);
	if (Program == "test-loop")
		test_test_loop(M);
	if (Program == "test-loop-2")
		test_test_loop_2(M);
	if (Program == "test-reducer")
		test_test_reducer(M);
	if (Program == "test-bound")
		test_test_bound(M);
	if (Program == "test-thread")
		test_test_thread(M);
	if (Program == "test-thread-2")
		test_test_thread_2(M);
	if (Program == "test-array")
		test_test_array(M);
	if (Program == "test-malloc")
		test_test_malloc(M);
	if (Program == "test-range")
		test_test_range(M);
	if (Program == "test-range-2")
		test_test_range_2(M);
	if (Program == "test-range-3")
		test_test_range_3(M);
	if (Program == "test-range-4")
		test_test_range_4(M);
	if (Program == "test-dep")
		test_test_dep(M);
	if (Program == "test-ctxt-2")
		test_test_ctxt_2(M);
	if (Program == "test-ctxt-4")
		test_test_ctxt_4(M);
	return false;
}
