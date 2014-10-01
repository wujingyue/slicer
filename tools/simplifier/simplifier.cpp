/**
 * Author: Jingyue
 *
 * Borrowed most of the code from opt.cpp.
 * Follow the LLVM coding style. 
 *
 * Note: DEBUG is ignored until ParseCommandLineOptions. 
 */

#define DEBUG_TYPE "simplifier"

#include <cstdio>
#include <memory>
#include <algorithm>
#include <sstream>
#include <string>
using namespace std;

#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/PassManager.h"
#include "llvm/CallGraphSCCPass.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Assembly/PrintModulePass.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/PassNameParser.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/SystemUtils.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/LinkAllVMCore.h"
using namespace llvm;

#include "listener.h"
using namespace slicer;

static SimplifierListener Listener;

/**
 * Use this option instead of printing to stdout. 
 * The program will remove the output file on failure. 
 * This saves lots of troubles for Makefiles. 
 */
static cl::opt<string> OutputFilename("o",
		cl::desc("Override output filename"),
		cl::value_desc("filename"),
		cl::init("-"));
static cl::opt<bool> UnitAtATime("funit-at-a-time",
		cl::desc("Enable IPO. This is same as llvm-gcc's -funit-at-a-time"),
		cl::init(true));
static cl::opt<bool> PrintAfterEachIteration("p",
		cl::desc("Print module after each iteration"));
static cl::opt<int> MaxIterNo("max-iter",
		cl::desc("Maximum number of iterations"),
		cl::init(-1));

void AddPass(PassManager &PM, Pass *P) {
	PM.add(P);
	PM.add(createVerifierPass());
}

/**
 * AddOptimizationPasses - This routine adds optimization passes
 * based on selected optimization level, OptLevel. This routine
 * duplicates llvm-gcc behaviour.
 *
 * OptLevel - Optimization Level
 */
void AddOptimizationPasses(PassManager &MPM, FunctionPassManager &FPM,
		unsigned OptLevel) {
  llvm::PassManagerBuilder builder;
  builder.OptLevel = OptLevel;

  builder.Inliner = llvm::createFunctionInliningPass();

  builder.DisableUnitAtATime = false;
  builder.DisableUnrollLoops = OptLevel == 0;

  builder.populateFunctionPassManager(FPM);
  builder.populateModulePassManager(MPM);
}

/**
 * Loads the RegisterPass's at the same time.
 * The events of loading RegisterPass's will be captured by SimpliferListener. 
 * Returns 0 on success, and 1 on failure. 
 */
int LoadPlugins() {
	const char *LLVMRoot = getenv("LLVM_ROOT");
	if (!LLVMRoot) {
		errs() << "Environment variable LLVM_ROOT is not set\n";
		return -1;
	}
	
	string LibDir = string(LLVMRoot) + "/install/lib";
	PluginLoader Loader;
	Loader = LibDir + "/id.so";
	Loader = LibDir + "/bc2bdd.so";
	Loader = LibDir + "/cfg.so";
	Loader = LibDir + "/slicer-trace.so";
	Loader = LibDir + "/max-slicing.so";
	Loader = LibDir + "/int.so";
	Loader = LibDir + "/reducer.so";

	return 0;
}

int Setup(int argc, char *argv[]) {
	// NOTE: I'm not able to find its definition. 
	sys::PrintStackTraceOnErrorSignal();
	// Enable debug stream buffering.
	EnableDebugBuffering = true;
	
	// Load plugins. 
	if (LoadPlugins() == -1)
		return -1;

	// Parse command line options after loading plugins. Otherwise, we wouldn't
	// be able to parse the options defined in the plugins. 
	cl::ParseCommandLineOptions(
			argc, argv, "Iteratively simplifies a max-sliced program\n");

	// Remove previous intermediate files. 
	for (int IterNo = 1; ; ++IterNo) {
		ostringstream OSS;
		OSS << "iter-" << IterNo << ".bc";
		if (remove(OSS.str().c_str()) == -1)
			break;
		DEBUG(dbgs() << "Removed " << OSS.str() << "\n";);
	}

	// Make sure that the Output file gets unlinked from the disk
	// if we get a SIGINT.
	sys::RemoveFileOnSignal(sys::Path(OutputFilename));

	return 0;
}

Module *LoadModuleFromStdin() {
	// Load the input module...
	SMDiagnostic Err;
	Module *M = ParseIRFile("-", Err, getGlobalContext());
	return M;
}

/*
 * Returns -1 on failure. 
 * Returns 1 if <M> gets changed. 
 * Returns 0 if <M> is unchanged. 
 *
 * NOTE: -O3 changes the module even if the module is already O3'ed. 
 * We shouldn't rely on the return value. 
 */
int RunOptimizationPasses(Module *M) {
	// Create a PassManager to hold and optimize the collection of passes we are
	// about to build...
	PassManager Passes;

	// TODO: Not sure if TargetData is necessary. 
	// Add an appropriate TargetData instance for this module...
	TargetData *TD = 0;
	const std::string &ModuleDataLayout = M->getDataLayout();
	if (!ModuleDataLayout.empty())
		TD = new TargetData(ModuleDataLayout);
	if (TD)
		Passes.add(TD);

	FunctionPassManager *FPasses = NULL;
	FPasses = new FunctionPassManager(M);
	if (TD)
		FPasses->add(new TargetData(*TD));

	AddOptimizationPasses(Passes, *FPasses, 3);

	bool changed = false;
	/*
	 * Run intra-procedural opts first.
	 * We could also use just one pass manager, but then we would have
	 * to be very careful about the order in which the passes are added
	 * (e.g. Add FunctionPass's before ModulePass's). 
	 */
	FPasses->doInitialization();
	for (Module::iterator I = M->begin(), E = M->end(); I != E; ++I)
		changed |= FPasses->run(*I);

	// Now that we have all of the passes ready, run them.
	changed |= Passes.run(*M);

	return (changed ? 1 : 0);
}

int RunPasses(Module *M, const vector<Pass *> &Passes) {
	PassManager PM;
	for (size_t i = 0; i < Passes.size(); ++i)
		AddPass(PM, Passes[i]);
	return (PM.run(*M) ? 1 : 0);
}

int RunLCSSAAndLoopSimplify(Module *M) {
	bool EverChanged;
	int Changed;
	do {
		EverChanged = false;
		Changed = RunPasses(M, vector<Pass *>(1, createLCSSAPass()));
		if (Changed == -1)
			return -1;
		EverChanged |= (Changed == 1);
		Changed = RunPasses(M, vector<Pass *>(1, createLoopSimplifyPass()));
		if (Changed == -1)
			return -1;
		EverChanged |= (Changed == 1);
	} while (EverChanged);
	return 0;
}

/*
 * Run the passes in <PIs> in the specified order. 
 *
 * Returns -1 on failure. 
 * Returns 1 if <M> gets changed. 
 * Returns 0 if <M> is unchanged. 
 */
int RunPassInfos(Module *M, const vector<const PassInfo *> &PIs) {
	// Create a PassManager to hold and optimize the collection of passes we are
	// about to build...
	PassManager Passes;
	
	// Add an appropriate TargetData instance for this module...
	TargetData *TD = 0;
	const std::string &ModuleDataLayout = M->getDataLayout();
	if (!ModuleDataLayout.empty())
		TD = new TargetData(ModuleDataLayout);
	if (TD)
		Passes.add(TD);
	
	for (size_t i = 0; i < PIs.size(); ++i) {
		const PassInfo *PI = PIs[i];
		if (!PI->getNormalCtor()) {
			errs() << "Cannot create Pass " << PI->getPassName() << "\n";
			return -1;
		}
		AddPass(Passes, PI->getNormalCtor()());
	}

	// Now that we have all of the passes ready, run them.
	bool Changed = Passes.run(*M);
	return (Changed ? 1 : 0);
}

int OutputModule(Module *M, const string &FileName) {
	raw_ostream *Out = &outs();  // Default to printing to stdout...
	if (FileName != "-") {
		string ErrorInfo;
		Out = new raw_fd_ostream(
				FileName.c_str(), ErrorInfo, raw_fd_ostream::F_Binary);
		if (!ErrorInfo.empty()) {
			errs() << ErrorInfo << '\n';
			delete Out;
			Out = NULL;
			return -1;
		}
	}

	WriteBitcodeToFile(M, *Out);

	// Delete the raw_fd_ostream. 
	if (Out != &outs()) {
		delete Out;
		Out = NULL;
	}

	return 0;
}

/**
 * Returns -1 on failure. 
 * Returns 1 if <M> gets changed. 
 * Returns 0 if <M> is unchanged. 
 */
int DoOneIteration(Module *M, int IterNo) {
	bool Changed = false;

	if (IterNo > 0) {
		// Run the AggressivePromotion to aggresively hoist LoadInst's. 
		if (const PassInfo *PI = Listener.getPassInfo("remove-assert-eq")) {
			if (RunPassInfos(M, vector<const PassInfo *>(1, PI)) == -1)
				return -1;
		} else {
			errs() << "AsesrtEqRemover hasn't been loaded.\n";
			return -1;
		}
		// LCSSA is actually not necessary in this step. 
		// aggressive-promotion only requires the loops to be in the simplified form. 
		if (RunLCSSAAndLoopSimplify(M) == -1)
			return -1;
		if (const PassInfo *PI = Listener.getPassInfo("aggressive-promotion")) {
			if (RunPassInfos(M, vector<const PassInfo *>(1, PI)) == -1)
				return -1;
		} else {
			errs() << "AggressivePromotion hasn't been loaded.\n";
			return -1;
		}
		// aggressive-loop-unroll requires the loops to be in LCSSA and
		// simplified form. 
		if (RunLCSSAAndLoopSimplify(M) == -1)
			return -1;
		if (const PassInfo *PI = Listener.getPassInfo("aggressive-loop-unroll")) {
			if (RunPassInfos(M, vector<const PassInfo *>(1, PI)) == -1)
				return -1;
		} else {
			errs() << "AggressiveLoopUnroll hasn't been loaded.\n";
			return -1;
		}
		// Run -O3 again to remove unnecessary instructions/BBs inserted
		// by LoopSimplifier and LCSSA. 
		if (RunOptimizationPasses(M) == -1)
			return -1;
		// CaptureConstraints requires all loops in LCSSA and simplified form. 
		if (RunLCSSAAndLoopSimplify(M) == -1)
			return -1;
		// As a side effect of PrintAfterEachIteration, print the module before
		// the integer constraint solving. 
		if (PrintAfterEachIteration) {
			if (OutputModule(M, "before-reducer.bc") == -1)
				return -1;
		}
		/*
		 * PostReducer requires Iterate, so don't worry about the Iterate. 
		 */
		if (const PassInfo *PI = Listener.getPassInfo("constantize")) {
			int Ret = RunPassInfos(M, vector<const PassInfo *>(1, PI));
			if (Ret == -1)
				return -1;
			if (Ret == 1)
				Changed = true;
		} else {
			errs() << "Constantizer hasn't been loaded.\n";
			return -1;
		}
	} // if IterNo > 0

	if (RunOptimizationPasses(M) == -1)
		return -1;
	if (RunLCSSAAndLoopSimplify(M) == -1)
		return -1;

	if (PrintAfterEachIteration) {
		ostringstream OSS;
		OSS << "iter-" << IterNo << ".bc";
		if (OutputModule(M, OSS.str()) == -1)
			return -1;
	}

	return ((IterNo == 0 || Changed) ? 1 : 0);
}

int main(int argc, char *argv[]) {
	/*
	 * X and Y must be put in the main function, because they can only
	 * be deleted when the program exits. 
	 * For the same reason, we shouldn't use function exit() anywhere
	 * in the program. That would skip the destructor of these two
	 * classes. 
	 */
	llvm::PrettyStackTraceProgram X(argc, argv);
	// Call llvm_shutdown() on exit.
	llvm_shutdown_obj Y;
	if (Setup(argc, argv) == -1)
		return 1;

	Module *M = LoadModuleFromStdin();
	if (!M)
		return 1;

	TimerGroup TG("Simplifier");
	vector<Timer *> Tmrs;
	bool Failed = false;
	
	for (int IterNo = 0; MaxIterNo == -1 || IterNo <= MaxIterNo; ++IterNo) {
		ostringstream OSS;
		OSS << "Iteration " << IterNo;
		Timer *TmrIter = new Timer(OSS.str(), TG);
		Tmrs.push_back(TmrIter);
		TmrIter->startTimer();

		dbgs() << "=== simplifier is starting Iteration " << IterNo << "... ===\n";
		int Changed = DoOneIteration(M, IterNo);
		dbgs() << "=== Iteration " << IterNo << " finished === ";

		if (Changed == 1)
			dbgs() << "[Changed]";
		else if (Changed == 0)
			dbgs() << "[Unchanged]";
		dbgs() << "\n";
		TmrIter->stopTimer();
		
		if (Changed == -1) {
			Failed = true;
			break;
		}

		if (Changed == 0)
			break;
	}

	for (size_t i = 0; i < Tmrs.size(); ++i)
		delete Tmrs[i];

	if (Failed) {
		delete M;
		return 1;
	}

	if (OutputModule(M, OutputFilename) == -1) {
		delete M;
		return 1;
	}

	delete M;
	return 0;
}
