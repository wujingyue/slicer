/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "int-constraints"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/LLVMContext.h"
#include "idm/id.h"
#include "common/include/util.h"
#include "common/include/typedefs.h"
#include "common/callgraph-fp/callgraph-fp.h"
#include "common/cfg/reach.h"
#include "common/cfg/intra-reach.h"
#include "common/cfg/icfg.h"
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
#include "exec-once.h"
#include "../max-slicing/clone-map-manager.h"
using namespace slicer;

static RegisterPass<CaptureConstraints> X(
		"capture",
		"Capture all integer constraints",
		false,
		true); // is analysis

STATISTIC(num_integers, "Number of integers");
STATISTIC(num_pointers, "Number of pointers");

char CaptureConstraints::ID = 0;

CaptureConstraints::CaptureConstraints(): ModulePass(&ID) {
#if 0
	n_symbols = 0;
#endif
	AA = NULL;
}

CaptureConstraints::~CaptureConstraints() {
	forall(vector<Clause *>, it, constraints) {
		delete *it;
		*it = NULL;
	}
}

void CaptureConstraints::print(raw_ostream &O, const Module *M) const {
#if 0
	O << "Start-BB bounds:\n";
	DenseMap<BasicBlock *, ValueBoundsInBB>::const_iterator it;
	for (it = start_bb_bounds.begin();
			it != start_bb_bounds.end(); ++it) {
		BasicBlock *bb = it->first;
		O << "BB " << bb->getParent()->getNameStr() << "."
			<< bb->getNameStr() << "\n";
		print_bounds_in_bb(O, it->second);
	}
#endif
#if 0
	O << "\nOverwriting:\n";
	map<int, vector<DenseSet<const ConstValueSet *> > >::const_iterator i, E;
	for (i = overwriting.begin(), E = overwriting.end(); i != E; ++i) {
		for (size_t trunk_id = 0; trunk_id < i->second.size(); ++trunk_id) {
			O << "Thread " << i->first << " Trunk " << trunk_id << ":\n";
			forallconst(DenseSet<const ConstValueSet *>, j, i->second[trunk_id])
				print_alias_set(O, *(*j));
		}
	}
#endif
	O << "\nConstants:\n";
	forallconst(ValueSet, it, constants) {
		print_value(O, *it);
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

bool CaptureConstraints::is_int_operation(unsigned opcode) {
	switch (opcode) {
		case Instruction::Add:
		case Instruction::Sub:
		case Instruction::Mul:
		case Instruction::UDiv:
		case Instruction::SDiv:
		case Instruction::URem:
		case Instruction::SRem:
		case Instruction::Shl:
		case Instruction::LShr:
		case Instruction::AShr:
		case Instruction::And:
		case Instruction::Or:
		case Instruction::Xor:
		case Instruction::ICmp:
		case Instruction::ZExt:
		case Instruction::SExt:
			return true;
		default:
			return false;
	}
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
	int_type = IntegerType::get(getGlobalContext(), 32);
	if (!AA)
		AA = &getAnalysis<BddAliasAnalysis>();
}

bool CaptureConstraints::runOnModule(Module &M) {

	setup(M);
	stat(M);

	constraints.clear();
	// Identify all integer and pointer constants.
	// Note that we define constants in a different way. 
	identify_constants(M);
	// Look at arithmetic operations on these constants. 
	capture_constraints_on_consts(M);
	// Look at loads and stores. 
	capture_addr_taken(M);
	// Collect constraints from unreachable blocks. 
	capture_unreachable(M);
	// Function summaries.
	// TODO: We'd better have a generic module for all function summaries
	// instead of writing it for each project. 
	capture_func_summaries(M);

	simplify_constraints();

	return false;
}

void CaptureConstraints::simplify_constraints() {
	// Sort constraints on the alphabetical order
	sort(
			constraints.begin(), constraints.end(),
			CompareClause(getAnalysis<ObjectID>()));
}

void CaptureConstraints::getAnalysisUsage(AnalysisUsage &AU) const {
	// TODO: do we need to use addRequiredTransitive for all passes? 
	// because CaptureConstraints.runOnModule is called in the iterator. 
	AU.setPreservesAll();
	AU.addRequiredTransitive<ObjectID>();
	AU.addRequired<DominatorTree>();
	AU.addRequired<IntraReach>();
	AU.addRequired<BddAliasAnalysis>();
	AU.addRequired<CallGraphFP>();
	AU.addRequired<ExecOnce>();
	// AU.addRequired<ICFGManager>();
	ModulePass::getAnalysisUsage(AU);
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
