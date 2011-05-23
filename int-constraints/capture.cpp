#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/LLVMContext.h"
#include "common/include/util.h"
#include "common/include/typedefs.h"
#include "common/reach/reach.h"
#include "common/reach/icfg.h"
#include "must-alias.h"
#include "../max-slicing-unroll/clone-map-manager.h"
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
	}

	void CaptureConstraints::print_value(raw_ostream &O, const Value *v) {
		if (isa<GlobalVariable>(v))
			O << "[global] ";
		else if (const Argument *arg = dyn_cast<Argument>(v))
			O << "[arg] (" << arg->getParent()->getNameStr() << ") ";
		else if (const Instruction *ins = dyn_cast<Instruction>(v))
			O << "[inst] (" << ins->getParent()->getParent()->getNameStr() << "." 
				<< ins->getParent()->getNameStr() << ") ";
		else
			assert(false && "Not supported");
		v->print(O);
		O << "\n";
	}

#if 0
	void CaptureConstraints::print_alias_set(
			raw_ostream &O, const ConstValueSet &as) {
		O << "Must-aliasing set:\n";
		forallconst(ConstValueSet, it, as)
			print_value(O, *it);
	}
#endif

	const Clause *CaptureConstraints::get_constraint(unsigned i) const {
		return constraints[i];
	}

	void CaptureConstraints::print_clause(
			raw_ostream &O, const Clause *c) {
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
			raw_ostream &O, const BoolExpr &be) {
		O << be.e1 << " <= " << be.e2;
	}

	void CaptureConstraints::print_constraint(
			raw_ostream &O, const Constraint &c) {
		O << c.first << " <= " << c.second;
	}

	void CaptureConstraints::print_bounds_in_bb(
			raw_ostream &O, const ValueBoundsInBB &bounds) {
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

	void CaptureConstraints::stat(Module &M) {
		unsigned n_ints = 0;
		forallfunc(M, fi) {
			for (Function::arg_iterator ai = fi->arg_begin();
					ai != fi->arg_end(); ++ai) {
				if (isa<IntegerType>(ai->getType()))
					++n_ints;
			}
			forall(Function, bi, *fi) {
				forall(BasicBlock, ii, *bi) {
					if (isa<IntegerType>(ii->getType()))
						++n_ints;
				}
			}
		}
		cerr << "# of integers = " << n_ints << endl;
	}

	bool CaptureConstraints::runOnModule(Module &M) {
		stat(M);
		// Collect constraints on top-level variables.
		// TODO: Handle function parameters. 
		forallfunc(M, fi) {
			if (!fi->isDeclaration())
				capture_in_func(fi);
		}
		// Collect constraints on address-taken variables. 
		// capture_addr_taken_vars(M);
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
		// FIXME: is it necessary? 
		AU.addRequired<DominatorTree>();
		AU.addRequired<ICFG>();
		ModulePass::getAnalysisUsage(AU);
	}

	unsigned CaptureConstraints::get_num_constraints() const {
		return 0;
	}

	char CaptureConstraints::ID = 0;
}

