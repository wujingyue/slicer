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
using namespace llvm;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace bc2bdd;

#include "common/util.h"
#include "common/exec-once.h"
using namespace rcs;

#include "slicer/iterate.h"
#include "slicer/capture.h"
#include "slicer/adv-alias.h"
#include "slicer/solve.h"
#include "slicer/test-utils.h"
#include "slicer/region-manager.h"
#include "int-test.h"
using namespace slicer;

static RegisterPass<IntTest> X("int-test",
		"Test the integer constraint solver", false, true);

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
	AU.addRequired<LoopInfo>();
#ifndef IDENTIFY_ONLY
	AU.addRequired<BddAliasAnalysis>();
	AU.addRequired<Iterate>();
	AU.addRequired<SolveConstraints>();
	AU.addRequired<CaptureConstraints>();
	AU.addRequired<AdvancedAlias>();
#endif
}

void IntTest::setup(Module &M) {
	int_type = IntegerType::get(M.getContext(), 32);
}

bool IntTest::runOnModule(Module &M) {
	setup(M);

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
		aget(M);
	if (Program == "aget-like")
		aget_like(M);
	if (Program == "test-overwrite")
		test_overwrite(M);
	if (Program == "test-overwrite-2")
		test_overwrite_2(M);
	if (Program == "FFT")
		fft(M);
	if (Program == "FFT-like")
		fft_like(M);
	if (Program == "RADIX")
		radix(M);
	if (Program == "RADIX-like")
		radix_like(M);
	if (Program == "LU-cont")
		lu_cont(M);
	if (Program == "pbzip2-like")
		pbzip2_like(M);
	if (Program == "blackscholes")
		blackscholes(M);
	if (Program == "ferret-like")
		ferret_like(M);
	if (Program == "test-loop")
		test_loop(M);
	if (Program == "test-loop-2")
		test_loop_2(M);
	if (Program == "test-reducer")
		test_reducer(M);
	if (Program == "test-bound")
		test_bound(M);
	if (Program == "test-thread")
		test_thread(M);
	if (Program == "test-thread-2")
		test_thread_2(M);
	if (Program == "test-array")
		test_array(M);
	if (Program == "test-malloc")
		test_malloc(M);
	if (Program == "test-range")
		test_range(M);
	if (Program == "test-range-2")
		test_range_2(M);
	if (Program == "test-range-3")
		test_range_3(M);
	if (Program == "test-range-4")
		test_range_4(M);
	if (Program == "test-dep")
		test_dep(M);
	if (Program == "test-ctxt-2")
		test_ctxt_2(M);
	if (Program == "test-ctxt-4")
		test_ctxt_4(M);
	if (Program == "test-global")
		test_global(M);
	if (Program == "test-lcssa")
		test_lcssa(M);
	if (Program == "test-barrier")
		test_barrier(M);
	if (Program == "test-path-2")
		test_path_2(M);
	if (Program == "test-alloca")
		test_alloca(M);
	return false;
}
