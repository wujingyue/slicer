/**
 * Author: Jingyue
 *
 * Borrowed most of the code from opt.cpp.
 * Follow the LLVM coding style. 
 */

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

/**
 * Use this option instead of printing to stdout. 
 * The program will remove the output file on failure. 
 * This saves lots of troubles for Makefiles. 
 */
static cl::opt<string> OutputFilename("o",
		cl::desc("Override output filename"),
		cl::value_desc("filename"),
		cl::init("-"));

void AddPass(PassManager &PM, Pass *P) {
	PM.add(P);
	PM.add(createVerifierPass());
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
	Loader = LibDir + "/libid-manager.so";
	Loader = LibDir + "/libmbb.so";
	Loader = LibDir + "/libbc2bdd.so";
	Loader = LibDir + "/libcallgraph-fp.so";
	Loader = LibDir + "/libcfg.so";
	Loader = LibDir + "/libslicer-trace.so";
	Loader = LibDir + "/libmax-slicing.so";
	Loader = LibDir + "/libint.so";
	Loader = LibDir + "/libreducer.so";
	DEBUG(dbgs() << "# of plugins = " << Loader.getNumPlugins() << "\n";);
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

	// Make sure that the Output file gets unlinked from the disk
	// if we get a SIGINT.
	sys::RemoveFileOnSignal(sys::Path(OutputFilename));

	return 0;
}

Module *LoadModuleFromStdin() {
	// Load the input module...
	SMDiagnostic Err;
	Module *M = ParseIRFile("-", Err, getGlobalContext());
	if (!M)
		Err.Print("repeated-lcssa", errs());
	return M;
}

/**
 * Run the passes in <Passes>. 
 */
int RunPasses(Module *M, const vector<Pass *> &Passes) {
	PassManager PM;
	for (size_t i = 0; i < Passes.size(); ++i)
		AddPass(PM, Passes[i]);
	return (PM.run(*M) ? 1 : 0);
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

	int Ret;
	do {
		Ret = RunPasses(M, vector<Pass *>(1, createLCSSAPass()));
		if (Ret == -1)
			return 1;
	} while (Ret == 1);
	
	if (RunPasses(M, vector<Pass *>(1, createLoopSimplifyPass())) == -1)
		return 1;

	if (OutputModule(M, OutputFilename) == -1) {
		delete M;
		return 1;
	}

	delete M;
	return 0;
}
