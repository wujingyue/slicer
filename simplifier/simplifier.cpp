/**
 * Author: Jingyue
 *
 * Borrowed most of the code from opt.cpp.
 * Follow the LLVM coding style. 
 */

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
#include "llvm/System/Signals.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/StandardPasses.h"
#include "llvm/Support/SystemUtils.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/LinkAllVMCore.h"
using namespace llvm;

#include <memory>
#include <algorithm>
#include <sstream>
#include <string>
using namespace std;

#include "config.h"
#include "listener.h"
using namespace slicer;

static SimplifierListener listener;
static raw_ostream *Out = NULL;

/**
 * Use this option instead of printing to stdout. 
 * The program will remove the output file on failure. 
 * This saves lots of troubles for Makefiles. 
 */
static cl::opt<string> OutputFilename(
		"o",
		cl::desc("Override output filename"),
		cl::value_desc("filename"),
		cl::init("-"));

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
	createStandardFunctionPasses(&FPM, OptLevel);

	llvm::Pass *InliningPass = 0;
	if (OptLevel) {
		unsigned Threshold = 200;
		if (OptLevel > 2)
			Threshold = 250;
		InliningPass = createFunctionInliningPass(Threshold);
	} else {
		InliningPass = createAlwaysInlinerPass();
	}
	/*
	 * We need to preserve all the cloned landmarks, therefore we disable
	 * simplifying lib calls. 
	 */
	createStandardModulePasses(&MPM, OptLevel,
			/*OptimizeSize=*/ false,
			/*UnitAtATime=*/ true,
			/*UnrollLoops=*/ OptLevel > 1,
			/*SimplifyLibCalls=*/ false,
			/*HaveExceptions=*/ true,
			InliningPass);
}

/*
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
	Loader = LibDir + "/libidm.so";
	Loader = LibDir + "/libid-manager.so";
	Loader = LibDir + "/libbc2bdd.so";
	Loader = LibDir + "/libcallgraph-fp.so";
	Loader = LibDir + "/libcfg.so";
	Loader = LibDir + "/libslicer-trace.so";
	Loader = LibDir + "/libmax-slicing.so";
	Loader = LibDir + "/libint.so";
	Loader = LibDir + "/libremove-br.so";
	errs() << "# of plugins = " << Loader.getNumPlugins() << "\n";
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

	Out = &outs();  // Default to printing to stdout...
	if (OutputFilename != "-") {
		// Make sure that the Output file gets unlinked from the disk if we get a
		// SIGINT
		sys::RemoveFileOnSignal(sys::Path(OutputFilename));

		string ErrorInfo;
		Out = new raw_fd_ostream(OutputFilename.c_str(), ErrorInfo,
				raw_fd_ostream::F_Binary);
		if (!ErrorInfo.empty()) {
			errs() << ErrorInfo << '\n';
			delete Out;
			Out = NULL;
			return -1;
		}
	}
	return 0;
}

Module *LoadModuleFromStdin() {
	// Load the input module...
	SMDiagnostic Err;
	Module *M = ParseIRFile("-", Err, getGlobalContext());
	if (!M)
		Err.Print("simplifier", errs());
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

/*
 * Run the passes in <PIs> in the specified order. 
 *
 * Returns -1 on failure. 
 * Returns 1 if <M> gets changed. 
 * Returns 0 if <M> is unchanged. 
 */
int RunPasses(Module *M, const vector<const PassInfo *> &PIs) {

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

/*
 * Returns -1 on failure. 
 * Returns 1 if <M> gets changed. 
 * Returns 0 if <M> is unchanged. 
 */
int DoOneIteration(Module *M) {

	if (RunOptimizationPasses(M) == -1)
		return -1;
	
	// Run BranchRemover. 
	// BranchRemover requires Iterator, so don't worry about Iterator. 
	const PassInfo *PI = listener.getBranchRemover();
	if (!PI) {
		errs() << "BranchRemover hasn't been loaded.\n";
		return -1;
	}
	
	// Optimization passes seem to always change the module (maybe a bug
	// in LLVM 2.7), so we only look at whether BranchRemover has changed
	// the module or not. 
	return RunPasses(M, vector<const PassInfo *>(1, PI));
}

int OutputModule(Module *M) {

	PassManager Passes;

	// Add an appropriate TargetData instance for this module...
	TargetData *TD = 0;
	const string &ModuleDataLayout = M->getDataLayout();
	if (!ModuleDataLayout.empty())
		TD = new TargetData(ModuleDataLayout);
	if (TD)
		Passes.add(TD);
	
	// Write bitcode or assembly out to disk or outs() as the last step...
	if (!Out) {
		errs() << "The output stream is not created.\n";
		return -1;
	}
	AddPass(Passes, createBitcodeWriterPass(*Out));
	
	// Now that we have all of the passes ready, run them.
	Passes.run(*M);

	// Delete the raw_fd_ostream. 
	if (Out != &outs()) {
		delete Out;
		Out = NULL;
	}

	return 0;
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

	int Changed;
	int IterNo = 0;
	TimerGroup TG("Simplifier");
	vector<Timer *> Tmrs;
	do {
		++IterNo;
		ostringstream OSS;
		OSS << "Iteration " << IterNo;
		Timer *TmrIter = new Timer(OSS.str(), TG);
		Tmrs.push_back(TmrIter);
		TmrIter->startTimer();
		errs() << "=== Starting Iteration " << IterNo << "... ===\n";
		Changed = DoOneIteration(M);
		errs() << "=== Iteration " << IterNo << " finished. ===\n";
		TmrIter->stopTimer();
		if (Changed == -1)
			return 1;
	} while (Changed);
	for (size_t i = 0; i < Tmrs.size(); ++i)
		delete Tmrs[i];

	if (OutputModule(M) == -1)
		return 1;

	return 0;
}
