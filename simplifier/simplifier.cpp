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
#include "llvm/Support/raw_ostream.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/LinkAllVMCore.h"
using namespace llvm;

#include <memory>
#include <algorithm>
using namespace std;

#include "config.h"
#include "listener.h"
using namespace slicer;

static SimplifierListener listener;

#if 0
static cl::list<const PassInfo*, bool, PassNameParser> PassList(
		cl::desc("Optimizations available:"));
#endif

/// AddOptimizationPasses - This routine adds optimization passes
/// based on selected optimization level, OptLevel. This routine
/// duplicates llvm-gcc behaviour.
///
/// OptLevel - Optimization Level
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
	createStandardModulePasses(&MPM, OptLevel,
			/*OptimizeSize=*/ false,
			/*UnitAtATime=*/ true,
			/*UnrollLoops=*/ OptLevel > 1,
			/*SimplifyLibCalls=*/ true,
			/*HaveExceptions=*/ true,
			InliningPass);
}

void Setup(int argc, char *argv[]) {
	// NOTE: I'm not able to find its definition. 
	sys::PrintStackTraceOnErrorSignal();
	// Enable debug stream buffering.
	EnableDebugBuffering = true;
	cl::ParseCommandLineOptions(
			argc, argv, "Iteratively simplifies a max-sliced program\n");
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
	Loader = LibDir + "/libbc2bdd.so";
	Loader = LibDir + "/libcallgraph-fp.so";
	Loader = LibDir + "/libcfg.so";
	Loader = LibDir + "/libint-constraints.so";
	Loader = LibDir + "/libremove-br.so";
	errs() << "# of plugins = " << Loader.getNumPlugins() << "\n";
	return 0;
}

/*
 * Returns -1 on failure. 
 * Returns 1 if <M> gets changed. 
 * Returns 0 if <M> is unchanged. 
 */
int DoOneIteration(Module *M) {

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

	FunctionPassManager *FPasses = NULL;
	FPasses = new FunctionPassManager(M);
	if (TD)
		FPasses->add(new TargetData(*TD));

#if 0
	// Add -O3. 
	AddOptimizationPasses(Passes, *FPasses, 3);
#endif
#if 1
	// Add BranchRemover. BranchRemover requires Iterator. 
	const PassInfo *PI = listener.getBranchRemover();
	if (!PI) {
		errs() << "BranchRemover hasn't been loaded.\n";
		return -1;
	}
	if (!PI->getNormalCtor()) {
		errs() << "Cannot create Pass " << PI->getPassName() << "\n";
		return -1;
	}
	Passes.add(PI->getNormalCtor()());
#endif

	FPasses->doInitialization();
	for (Module::iterator I = M->begin(), E = M->end(); I != E; ++I)
		FPasses->run(*I);

	// Now that we have all of the passes ready, run them.
	if (Passes.run(*M))
		return 1;
	else
		return 0;
}

void OutputModule(Module *M) {
	PassManager Passes;
	// Add an appropriate TargetData instance for this module...
	TargetData *TD = 0;
	const std::string &ModuleDataLayout = M->getDataLayout();
	if (!ModuleDataLayout.empty())
		TD = new TargetData(ModuleDataLayout);
	if (TD)
		Passes.add(TD);
	// Write bitcode or assembly out to disk or outs() as the last step...
	Passes.add(createBitcodeWriterPass(outs()));
	// Now that we have all of the passes ready, run them.
	Passes.run(*M);
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
	Setup(argc, argv);

	Module *M = LoadModuleFromStdin();
	if (!M)
		return 1;

	if (LoadPlugins() == -1) {
		delete M;
		return 1;
	}

	int changed;
	do {
#ifdef VERBOSE
		errs() << "===== Iteration =====\n";
#endif
		changed = DoOneIteration(M);
		if (changed == -1)
			return 1;
	} while (changed);

	OutputModule(M);

	return 0;
}
