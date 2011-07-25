/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "int"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/LLVMContext.h"
#include "llvm/Target/TargetData.h"
#include "idm/id.h"
#include "common/include/util.h"
#include "common/include/typedefs.h"
#include "common/callgraph-fp/callgraph-fp.h"
#include "common/cfg/intra-reach.h"
#include "common/cfg/icfg.h"
#include "common/cfg/exec-once.h"
#include "common/cfg/partial-icfg-builder.h"
using namespace llvm;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include <fstream>
#include <sstream>
#include <locale>
using namespace std;

#include "config.h"
#include "capture.h"
#include "must-alias.h"
#include "trace/landmark-trace.h"
#include "max-slicing/clone-info-manager.h"
#include "max-slicing/region-manager.h"
using namespace slicer;

static RegisterPass<CaptureConstraints> X(
		"capture",
		"Capture all integer constraints",
		false,
		true); // is analysis

STATISTIC(num_integers, "Number of integers");
STATISTIC(num_pointers, "Number of pointers");

char CaptureConstraints::ID = 0;

void CaptureConstraints::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequiredTransitive<TargetData>();
	AU.addRequiredTransitive<LoopInfo>();
	AU.addRequiredTransitive<ObjectID>();
	AU.addRequiredTransitive<DominatorTree>();
	AU.addRequiredTransitive<IntraReach>();
	AU.addRequired<BddAliasAnalysis>(); // Only used in <setup>. 
	AU.addRequiredTransitive<CallGraphFP>();
	AU.addRequiredTransitive<ExecOnce>();
	AU.addRequiredTransitive<LandmarkTrace>();
	AU.addRequiredTransitive<CloneInfoManager>();
	AU.addRequiredTransitive<RegionManager>();
	AU.addRequiredTransitive<PartialICFGBuilder>();
	AU.addRequiredTransitive<MicroBasicBlockBuilder>();
	ModulePass::getAnalysisUsage(AU);
}

CaptureConstraints::CaptureConstraints(): ModulePass(&ID), IDT(false) {
	AA = NULL;
}

CaptureConstraints::~CaptureConstraints() {
	forall(vector<Clause *>, it, constraints) {
		delete *it;
		*it = NULL;
	}
}

void CaptureConstraints::print(raw_ostream &O, const Module *M) const {
	ObjectID &OI = getAnalysis<ObjectID>();
	O << "\nIntegers:\n";
	forallconst(ConstValueSet, it, integers) {
		unsigned value_id = OI.getValueID(*it);
		assert(value_id != ObjectID::INVALID_ID);
		O << "  x" << value_id << "\n";
	}
	O << "\nConstraints:\n";
	forallconst(vector<Clause *>, it, constraints) {
		print_clause(O, *it, getAnalysis<ObjectID>());
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
	forallfunc(M, fi) {
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
	assert(AA == NULL);
	AA = &getAnalysis<BddAliasAnalysis>();
}

bool CaptureConstraints::runOnModule(Module &M) {
	setup(M);
	stat(M);
	return recalculate(M);
}

void CaptureConstraints::check_loop(Loop *l) {
	assert(l->isLCSSAForm());
	assert(l->isLoopSimplifyForm());
	for (Loop::iterator li = l->begin(); li != l->end(); ++li)
		check_loop(*li);
}

void CaptureConstraints::check_loops(Module &M) {
	forallfunc(M, f) {
		if (f->isDeclaration())
			continue;
		LoopInfo &LI = getAnalysis<LoopInfo>(*f);
		for (LoopInfo::iterator lii = LI.begin(); lii != LI.end(); ++lii) {
			Loop *l = *lii;
			check_loop(l);
		}
	}
}

bool CaptureConstraints::recalculate(Module &M) {

	constraints.clear();

	check_loops(M);
	
	// Identify all integer and pointer variables. 
	identify_integers(M);
	
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
	capture_func_summaries(M);

	simplify_constraints();
#ifdef VERBOSE
	errs() << "# of constraints = " << get_num_constraints() << "\n";
#endif

	return false;
}

void CaptureConstraints::simplify_constraints() {
	/*
	 * Sort constraints on the alphabetical order
	 * so that get_fingerprint will return the same value deterministically
	 * for the same set of constraints. 
	 */
	sort(
			constraints.begin(), constraints.end(),
			CompareClause(getAnalysis<ObjectID>()));
}

unsigned CaptureConstraints::get_num_constraints() const {
	return (unsigned)constraints.size();
}

long CaptureConstraints::get_fingerprint() const {
	long res = 0;
	ObjectID &OI = getAnalysis<ObjectID>();
	locale loc;
	const collate<char> &coll = use_facet<collate<char> >(loc);
	for (size_t i = 0; i < constraints.size(); ++i) {
		string str;
		raw_string_ostream oss(str);
		print_clause(oss, constraints[i], OI);
		res += coll.hash(oss.str().data(), oss.str().data() + oss.str().length());
	}
	return res;
}

bool CaptureConstraints::print_progress(
		raw_ostream &O, unsigned cur, unsigned tot) {
	bool printed = false;
	for (unsigned p = 0; p < 10; ++p) {
		unsigned threshold = p * tot / 10;
		if (cur == threshold) {
			O << "========== " << p * 10 << "%" << " ==========\n";
			printed = true;
			// Do not return here. We may need to print multiple percentages if
			// tot is small. 
		}
	}
	return printed;
}
