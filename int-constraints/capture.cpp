#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/LLVMContext.h"
#include "idm/id.h"
#include "common/include/util.h"
#include "common/include/typedefs.h"
using namespace llvm;

#include <fstream>
#include <sstream>
using namespace std;

#include "config.h"
#include "capture.h"

namespace {
	static RegisterPass<slicer::CaptureConstraints> X(
			"capture-constraints",
			"Capture all integer constraints",
			false,
			true); // is analysis
}

namespace slicer {

	CaptureConstraints::CaptureConstraints(): ModulePass(&ID), n_symbols(0) {}

	CaptureConstraints::~CaptureConstraints() {
		forall(vector<Clause *>, it, constraints) {
			delete *it;
			*it = NULL;
		}
	}

	void CaptureConstraints::print(raw_ostream &O, const Module *M) const {
		O << "Start-BB bounds:\n";
		DenseMap<BasicBlock *, ValueBoundsInBB>::const_iterator it;
		for (it = start_bb_bounds.begin();
				it != start_bb_bounds.end(); ++it) {
			BasicBlock *bb = it->first;
			O << "BB " << bb->getParent()->getNameStr() << "."
				<< bb->getNameStr() << "\n";
			print_bounds_in_bb(O, it->second);
		}
		O << "\nConstraints:\n";
		forallconst(vector<Clause *>, it, constraints) {
			print_clause(O, *it);
			O << "\n";
		}
	}

	const Clause *CaptureConstraints::get_constraint(unsigned i) const {
		return constraints[i];
	}

	void CaptureConstraints::print_clause(
			raw_ostream &O, const Clause *c) const {
		if (c->op == Clause::None) {
			print_bool_expr(O, c->be);
			return;
		}
		O << "(";
		print_clause(O, c->c1);
		O << ") ";
		O << (c->op == Clause::And ? "AND" : "OR");
		O << " (";
		print_clause(O, c->c2);
		O << ")";
	}

	void CaptureConstraints::print_bool_expr(
			raw_ostream &O, const BoolExpr &be) const {
		O << be.e1 << " <= " << be.e2;
	}

	void CaptureConstraints::print_constraint(
			raw_ostream &O, const Constraint &c) const {
		O << c.first << " <= " << c.second;
	}

	void CaptureConstraints::print_bounds_in_bb(
			raw_ostream &O, const ValueBoundsInBB &bounds) const {
		forallconst(ValueBoundsInBB, it, bounds) {
			O << "\t";
			it->first->print(O);
			O << "\n";
			O << "\t[" << it->second.first << ", " << it->second.second << "]\n";
		}
	}

	string CaptureConstraints::get_symbol_name(unsigned sym_id) {
		ostringstream oss;
		oss << "x" << sym_id;
		return oss.str();
	}

	Expr CaptureConstraints::create_new_symbol() {
		Expr res = get_symbol_name(n_symbols);
		n_symbols++;
		return res;
	}

	Expr CaptureConstraints::get_lower_bound(
			Value *v, const ValueBoundsInBB &end_bb_bounds) {
		assert(end_bb_bounds.count(v));
		return end_bb_bounds.lookup(v).first;
	}

	Expr CaptureConstraints::get_upper_bound(
			Value *v, const ValueBoundsInBB &end_bb_bounds) {
		assert(end_bb_bounds.count(v));
		return end_bb_bounds.lookup(v).second;
	}

	Expr CaptureConstraints::get_const_expr(ConstantInt *v) {
		ostringstream oss;
		oss << v->getSExtValue();
		return oss.str();
	}

	bool CaptureConstraints::is_infty_large(const Expr &e) {
		return e == "infty";
	}

	bool CaptureConstraints::is_infty_small(const Expr &e) {
		return e == "-infty";
	}

	bool CaptureConstraints::is_infty(const Expr &e) {
		return is_infty_large(e) || is_infty_small(e);
	}

	Expr CaptureConstraints::get_infty_large() {
		return "infty";
	}

	Expr CaptureConstraints::get_infty_small() {
		return "-infty";
	}

	Expr CaptureConstraints::create_expr(
			const string &op, const Expr &op1, const Expr &op2) {
		return string("(") + op1 + " " + op + " " + op2 + ")";
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

	bool CaptureConstraints::runOnModule(Module &M) {
		// Collect constraints on top-level variables.
		forallfunc(M, fi) {
			if (!fi->isDeclaration())
				capture_in_func(fi);
		}
		// Collect constraints on address-taken variables. 
		simplify_constraints();
		return false;
	}

	void CaptureConstraints::simplify_constraints() {
		size_t i = 0;
		while (i < constraints.size()) {
			Clause *c = constraints[i];
			if (c->op == Clause::None && c->be.e1 == c->be.e2) {
				delete c;
				constraints.erase(constraints.begin() + i);
				continue;
			}
			++i;
		}
	}

	void CaptureConstraints::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<DominatorTree>();
		AU.addRequiredTransitive<ObjectID>();
		ModulePass::getAnalysisUsage(AU);
	}

	unsigned CaptureConstraints::get_num_constraints() const {
		return 0;
	}

	char CaptureConstraints::ID = 0;
}

