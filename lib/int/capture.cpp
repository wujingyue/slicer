/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "int"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/LLVMContext.h"
#include "llvm/Target/TargetData.h"
#include "common/util.h"
#include "common/typedefs.h"
#include "common/callgraph-fp.h"
#include "common/intra-reach.h"
#include "common/icfg.h"
#include "common/exec-once.h"
#include "common/partial-icfg-builder.h"
#include "bc2bdd/InitializePasses.h"
#include "common/InitializePasses.h"
#include "slicer/InitializePasses.h"
using namespace llvm;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace bc2bdd;

#include <fstream>
#include <sstream>
#include <locale>
using namespace std;

#include "slicer/capture.h"
#include "slicer/landmark-trace.h"
#include "slicer/clone-info-manager.h"
#include "slicer/region-manager.h"
#include "slicer/may-write-analyzer.h"
#include "slicer/stratify-loads.h"
using namespace slicer;

INITIALIZE_PASS_BEGIN(CaptureConstraints, "capture",
		"Capture all integer constraints", false, true)
INITIALIZE_PASS_DEPENDENCY(BddAliasAnalysis)
INITIALIZE_PASS_DEPENDENCY(TargetData)
INITIALIZE_PASS_DEPENDENCY(IDAssigner)
INITIALIZE_PASS_DEPENDENCY(DominatorTree)
INITIALIZE_PASS_DEPENDENCY(LoopInfo)
INITIALIZE_PASS_DEPENDENCY(IntraReach)
INITIALIZE_PASS_DEPENDENCY(CallGraphFP)
INITIALIZE_PASS_DEPENDENCY(ExecOnce)
INITIALIZE_PASS_DEPENDENCY(LandmarkTrace)
INITIALIZE_PASS_DEPENDENCY(CloneInfoManager)
INITIALIZE_PASS_DEPENDENCY(RegionManager)
INITIALIZE_PASS_DEPENDENCY(PartialICFGBuilder)
INITIALIZE_PASS_DEPENDENCY(MicroBasicBlockBuilder)
INITIALIZE_PASS_DEPENDENCY(MayWriteAnalyzer)
INITIALIZE_PASS_DEPENDENCY(StratifyLoads)
INITIALIZE_PASS_END(CaptureConstraints, "capture",
		"Capture all integer constraints", false, true)

void CaptureConstraints::getAnalysisUsage(AnalysisUsage &AU) const {
	// LLVM 2.9 crashes if I addRequiredTransitive FunctionPasses.
	AU.setPreservesAll();
	AU.addRequired<BddAliasAnalysis>(); // Only used in <setup>. 
	AU.addRequired<TargetData>();
	AU.addRequired<IDAssigner>();
	AU.addRequired<DominatorTree>();
	AU.addRequired<LoopInfo>();
	AU.addRequired<IntraReach>();
	AU.addRequired<CallGraphFP>();
	AU.addRequired<ExecOnce>();
	AU.addRequired<LandmarkTrace>();
	AU.addRequired<CloneInfoManager>();
	AU.addRequired<RegionManager>();
	AU.addRequired<PartialICFGBuilder>();
	AU.addRequired<MicroBasicBlockBuilder>();
	AU.addRequired<MayWriteAnalyzer>();
	AU.addRequired<StratifyLoads>();
}

static cl::opt<bool> DisableAllConstraints("disable-constraints",
		cl::desc("Don't capture any constraints"));

STATISTIC(num_integers, "Number of integers");
STATISTIC(num_pointers, "Number of pointers");

char CaptureConstraints::ID = 0;

CaptureConstraints::CaptureConstraints():
	ModulePass(ID), IDT(false), current_level((unsigned)-1)
{
	initializeCaptureConstraintsPass(*PassRegistry::getPassRegistry());
}

CaptureConstraints::~CaptureConstraints() {
	clear_constraints();
}

void CaptureConstraints::print(raw_ostream &O, const Module *M) const {
	IDAssigner &IDA = getAnalysis<IDAssigner>();
	O << "\nIntegers:\n";
	for (ValueSet::const_iterator it = fixed_integers.begin();
			it != fixed_integers.end(); ++it) {
		if (isa<ConstantInt>(*it))
			continue;
		unsigned value_id = IDA.getValueID(*it);
		if (value_id == IDAssigner::INVALID_ID)
			O << **it << "\n";
		assert(value_id != IDAssigner::INVALID_ID);
		O << "  x" << value_id << ":" << **it << "\n";
	}
	O << "\nConstraints:\n";
	for (unsigned i = 0; i < get_num_constraints(); ++i) {
		print_clause(O, get_constraint(i), getAnalysis<IDAssigner>());
		O << "\n";
	}
}

void CaptureConstraints::print_value(raw_ostream &O, const Value *v) {
	if (isa<GlobalVariable>(v))
		O << "[global] ";
	else if (const Argument *arg = dyn_cast<Argument>(v))
		O << "[arg] (" << arg->getParent()->getNameStr() << ") ";
	else if (const Instruction *ins = dyn_cast<Instruction>(v))
		O << "[inst] (" << ins->getParent()->getParent()->getNameStr() << "." 
			<< ins->getParent()->getNameStr() << ") ";
	else if (isa<Constant>(v))
		O << "[const] ";
	else
		assert(false && "Not supported");
	v->print(O);
	O << "\n";
}

const Clause *CaptureConstraints::get_constraint(unsigned i) const {
	return constraints[i];
}

void CaptureConstraints::stat(Module &M) {
	for (Module::iterator fi = M.begin(); fi != M.end(); ++fi) {
		for (Function::arg_iterator ai = fi->arg_begin();
				ai != fi->arg_end(); ++ai) {
			if (isa<IntegerType>(ai->getType()))
				++num_integers;
			if (isa<PointerType>(ai->getType()))
				++num_pointers;
		}
		forall(Function, bi, *fi) {
			forall(BasicBlock, ii, *bi) {
				if (isa<IntegerType>(ii->getType()))
					++num_integers;
				if (isa<PointerType>(ii->getType()))
					++num_pointers;
			}
		}
	}
	for (Module::global_iterator gi = M.global_begin();
			gi != M.global_end(); ++gi)
		++num_pointers;
}

void CaptureConstraints::setup(Module &M) {
	// int is always 32-bit long. 
	int_type = IntegerType::get(M.getContext(), 32);
}

bool CaptureConstraints::runOnModule(Module &M) {
	assert(!is_using_advanced_alias());
	setup(M);
	stat(M);
	calculate(M);
	return false;
}

void CaptureConstraints::recalculate(Module &M) {
	assert(is_using_advanced_alias());
	calculate(M);
}

void CaptureConstraints::clear_constraints() {
	for (vector<Clause *>::iterator it = constraints.begin();
			it != constraints.end(); ++it) {
		delete *it;
		*it = NULL;
	}
	constraints.clear();
}

void CaptureConstraints::calculate(Module &M) {
	clear_constraints();

	// Check whether each loop is in the simplified and LCSSA form. 
	check_loops(M);

	// Identify all integer and pointer variables. 
	identify_fixed_integers(M);
	// <fixed_integers> may be changed in <capture_addr_taken>. 

	// Look at arithmetic operations on these constants. 
	capture_top_level(M);
	// Look at loads and stores. 
	// The algorithm to capture address-taken variables are flow-sensitive.
	// Need compute the inter-procedural CFG before hand. 
	ICFG &PIB = getAnalysis<PartialICFGBuilder>();
	IDT.recalculate(PIB);
	capture_addr_taken(M);
	// Collect constraints from unreachable blocks. 
	capture_unreachable(M);
	// Function summaries.
	// TODO: We'd better have a generic module for all function summaries
	// instead of writing it for each project. 
	capture_function_summaries(M);

	simplify_constraints();
	dbgs() << "# of constraints = " << get_num_constraints() << "\n";
	if (DisableAllConstraints)
		constraints.clear();
}

void CaptureConstraints::simplify_constraints() {
	/*
	 * Sort constraints.
	 * so that get_fingerprint will return the same value deterministically
	 * for the same set of constraints. 
	 */
	sort(constraints.begin(), constraints.end(),
			CompareClause(getAnalysis<IDAssigner>()));
}

unsigned CaptureConstraints::get_num_constraints() const {
	unsigned res = constraints.size();
	return res;
}

long CaptureConstraints::get_fingerprint() const {
	long res = 0;
	IDAssigner &IDA = getAnalysis<IDAssigner>();
	locale loc;
	const collate<char> &coll = use_facet<collate<char> >(loc);
	for (size_t i = 0; i < constraints.size(); ++i) {
		string str;
		raw_string_ostream oss(str);
		print_clause(oss, constraints[i], IDA);
		res += coll.hash(oss.str().data(), oss.str().data() + oss.str().length());
	}
	return res;
}

bool CaptureConstraints::print_progress(
		raw_ostream &O, unsigned cur, unsigned tot) {
	bool printed = false;
	for (unsigned p = 0; p <= 10; ++p) {
		unsigned threshold = p * tot / 10;
		if (cur == threshold) {
			O << " [" << p * 10 << "%" << "]";
			printed = true;
			// Do not return here. We may need to print multiple percentages if
			// tot is small. 
		}
	}
	return printed;
}

void CaptureConstraints::add_constraint(Clause *c) {
	// TODO: Simplify the clause. 
	// e.g. Split the conjuction. 
	if (c)
		constraints.push_back(c);
}
