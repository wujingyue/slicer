/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "int"

#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"
#include "common/InitializePasses.h"
#include "slicer/InitializePasses.h"
using namespace llvm;

#include "common/util.h"
#include "common/callgraph-fp.h"
#include "common/exec-once.h"
using namespace rcs;

#include "slicer/stratify-loads.h"
using namespace slicer;

INITIALIZE_PASS_BEGIN(StratifyLoads, "stratify-loads",
		"Stratify load instructions", false, true)
INITIALIZE_PASS_DEPENDENCY(ExecOnce)
INITIALIZE_PASS_DEPENDENCY(CallGraphFP)
INITIALIZE_PASS_END(StratifyLoads, "stratify-loads",
		"Stratify load instructions", false, true)

void StratifyLoads::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequiredTransitive<ExecOnce>();
	AU.addRequired<CallGraphFP>();
}

static cl::opt<bool> DisableStratifying("disable-stratifying",
		cl::desc("All pointers are at Level 0"),
		cl::init(true));

char StratifyLoads::ID = 0;

StratifyLoads::StratifyLoads(): ModulePass(ID) {
	initializeStratifyLoadsPass(*PassRegistry::getPassRegistry());
}

bool StratifyLoads::try_calculating_levels(Module &M) {
	ExecOnce &EO = getAnalysis<ExecOnce>();
	CallGraphFP &CG = getAnalysis<CallGraphFP>();

	bool changed = false;
	
	// Go through each instruction. 
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				if (!EO.not_executed(ins) && EO.executed_once(ins)) {
					// Skip <ins> if its level is already computed. 
					if (level.count(ins))
						continue;

					if (LoadInst *li = dyn_cast<LoadInst>(ins)) {
						unsigned child_level = get_level(li->getPointerOperand());
						if (child_level != (unsigned)-1) {
							level[ins] = child_level + 1;
							changed = true;
						}
					} else if (is_call(ins)) {
						unsigned max_level = 0;
						FuncList callees = CG.get_called_functions(ins);
						for (unsigned i = 0; i < callees.size(); ++i) {
							Function *callee = callees[i];
							if (callee->isDeclaration()) {
								max_level = (unsigned)-1;
							} else {
								// Not a declaration. 
								if (!EO.not_executed(callee)) {
									for (Function::iterator bb = callee->begin();
											bb != callee->end(); ++bb) {
										if (ReturnInst *ri = dyn_cast<ReturnInst>(
													bb->getTerminator())) {
											if (Value *rv = ri->getReturnValue())
												max_level = max(max_level, get_level(rv));
										}
									}
								}
							}
						}
						if (max_level != (unsigned)-1) {
							level[ins] = max_level;
							changed = true;
						}
					} else {
						// Not function call nor load instruction. 
						unsigned max_level = 0;
						for (unsigned i = 0; i < ins->getNumOperands(); ++i)
							max_level = max(max_level, get_level(ins->getOperand(i)));
						if (max_level != (unsigned)-1) {
							level[ins] = max_level;
							changed = true;
						}
					}
				}
			}
		}
	}

	// Go through each function argument. 
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		// Skip external functions. 
		if (f->isDeclaration())
			continue;
		if (!EO.not_executed(f) && EO.executed_once(f)) {
			InstList call_sites = CG.get_call_sites(f);
			// Not all call sites are reachable. 
			for (size_t i = 0; i < call_sites.size(); ) {
				if (EO.not_executed(call_sites[i]))
					call_sites.erase(call_sites.begin() + i);
				else
					++i;
			}
			if (call_sites.size() > 1) {
				errs() << f->getName() << " has "
					<< call_sites.size() << " call sites.\n";
			}
			assert(call_sites.size() <= 1);
			if (call_sites.size() == 1) {
				CallSite cs(call_sites[0]);
				assert(cs.getInstruction());
				if (is_pthread_create(call_sites[0])) {
					// A thread function has only one argument. 
					if (1 == f->arg_size()) {
						// Skip <ai> if already computed. 
						if (level.count(f->arg_begin()))
							continue;
						unsigned child_level = get_level(
								get_pthread_create_arg(call_sites[0]));
						if (child_level != (unsigned)-1) {
							level[f->arg_begin()] = child_level;
							changed = true;
						}
					}
				} else {
					// Regular function calls. 
					if (cs.arg_size() == f->arg_size()) {
						// May not always be the case, e.g. bitcast. 
						// We give up in that case. 
						Function::arg_iterator ai = f->arg_begin();
						for (unsigned i = 0; i < cs.arg_size(); ++i) {
							// Skip <ai> if already computed. 
							if (level.count(ai))
								continue;
							unsigned child_level = get_level(cs.getArgument(i));
							if (child_level != (unsigned)-1) {
								level[ai] = child_level;
								changed = true;
							}
							++ai;
						}
					}
				}
			}
		}
	}

	return changed;
}

unsigned StratifyLoads::get_level(const Value *v) const {
	assert(v);

	if (DisableStratifying)
		return 0;

	if (!isa<Argument>(v) && !isa<Instruction>(v))
		return 0;

	DenseMap<Value *, unsigned>::const_iterator it = level.find(
			const_cast<Value *>(v));
	if (it == level.end())
		return (unsigned)-1;
	return it->second;
}

bool StratifyLoads::is_memory_allocation(Instruction *ins) {
	if (isa<AllocaInst>(ins))
		return true;

	CallSite cs(ins);
	if (!cs.getInstruction())
		return false;

	Function *callee = cs.getCalledFunction();
	if (!callee)
		return false;

	StringRef name = callee->getName();
	return name.startswith("_Zna") || name.startswith("_Znw") ||
		name == "malloc" || name == "valloc" || name == "calloc" ||
		name == "strdup";
}

bool StratifyLoads::runOnModule(Module &M) {
	// Initialize:
	// 1. globals and constants are top-level.
	// 2. mallocs (including whatever memory allocation call) are top-level.
	ExecOnce &EO = getAnalysis<ExecOnce>();
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				if (!EO.not_executed(ins) && EO.executed_once(ins) &&
						is_memory_allocation(ins)) {
					level[ins] = 0;
				}
			}
		}
	}

	while (try_calculating_levels(M)) {
		dbgs() << "Running in StratifyLoads...\n";
	}
	return false;
}

unsigned StratifyLoads::get_max_level() const {
	unsigned max_level = 0;
	for (DenseMap<Value *, unsigned>::const_iterator it = level.begin();
			it != level.end(); ++it) {
		assert(it->second != (unsigned)-1);
		max_level = max(max_level, it->second);
	}
	return max_level;
}

void StratifyLoads::print(raw_ostream &O, const Module *M) const {
	ExecOnce &EO = getAnalysis<ExecOnce>();

	vector<pair<unsigned, const Instruction *> > all_levels;
	for (Module::const_iterator f = M->begin(); f != M->end(); ++f) {
		for (Function::const_iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::const_iterator ins = bb->begin();
					ins != bb->end(); ++ins) {
				if (isa<LoadInst>(ins)) {
					if (EO.not_executed(ins))
						continue;
					all_levels.push_back(make_pair(get_level(ins), ins));
				}
			}
		}
	}
	sort(all_levels.begin(), all_levels.end());
	for (size_t i = 0; i < all_levels.size(); ++i) {
		O << all_levels[i].first << ":" << *all_levels[i].second << "\n";
	}
}
