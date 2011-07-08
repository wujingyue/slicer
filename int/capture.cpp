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
#include "../trace/landmark-trace.h"
#include "../max-slicing/clone-info-manager.h"
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
	AU.addRequiredTransitive<ObjectID>();
	AU.addRequiredTransitive<DominatorTree>();
	AU.addRequiredTransitive<IntraReach>();
	AU.addRequired<BddAliasAnalysis>(); // Only used in <setup>. 
	AU.addRequiredTransitive<CallGraphFP>();
	AU.addRequiredTransitive<ExecOnce>();
	AU.addRequiredTransitive<LandmarkTrace>();
	AU.addRequiredTransitive<CloneInfoManager>();
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
	O << "\nConstants:\n";
	forallconst(ConstValueSet, it, constants) {
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
	int_type = IntegerType::get(M.getContext(), 32);
	assert(AA == NULL);
	AA = &getAnalysis<BddAliasAnalysis>();
}

bool CaptureConstraints::runOnModule(Module &M) {
	setup(M);
	stat(M);
	return recalculate(M);
}

void CaptureConstraints::check_clone_info(Module &M) {
	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
	LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
	vector<int> thr_ids = LT.get_thr_ids();
	for (size_t k = 0; k < thr_ids.size(); ++k) {
		int i = thr_ids[k];
		size_t n_trunks = LT.get_n_trunks(i);
		for (size_t j = 0; j < n_trunks; ++j) {
			if (LT.is_enforcing_landmark(i, j)) {
				unsigned orig_ins_id = LT.get_landmark(i, j).ins_id;
				if (CIM.get_instruction(i, j, orig_ins_id) == NULL) {
					errs() << "(" << i << ", " << j << ", " <<
						orig_ins_id << ") not found\n";
					assert(false && "Some enforcing landmarks cannot be found.");
				}
			}
		}
	}
}

bool CaptureConstraints::recalculate(Module &M) {

	// Check clone_info metadata. 
	check_clone_info(M);

	constraints.clear();
	
	// Identify all integer and pointer constants.
	// Note that we define constants in a different way. 
	identify_constants(M);
	
	// Look at arithmetic operations on these constants. 
	capture_constraints_on_consts(M);
	
	// Look at loads and stores. 
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

	return false;
}

void CaptureConstraints::simplify_constraints() {
	// Sort constraints on the alphabetical order
	sort(
			constraints.begin(), constraints.end(),
			CompareClause(getAnalysis<ObjectID>()));
	// Some stats. 
	unsigned n_assignments = 0;
	for (size_t i = 0; i < constraints.size(); ++i) {
		const Clause *c = constraints[i];
		if (!c->be)
			continue;
		const BoolExpr *be = c->be;
		if (be->p != CmpInst::ICMP_EQ)
			continue;
		const Expr *e1 = be->e1, *e2 = be->e2;
		if (e1->type != Expr::SingleDef || e2->type != Expr::SingleDef)
			continue;
		++n_assignments;
	}
	errs() << n_assignments << " out of " << constraints.size() <<
		" constraints are assignments.\n";
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
