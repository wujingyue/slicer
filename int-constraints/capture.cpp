#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/Dominators.h"
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

	void CaptureConstraints::print(raw_ostream &O, const Module *M) const {
		DenseMap<BasicBlock *, ValueBoundsInBB>::const_iterator it;
		for (it = start_bb_bounds.begin();
				it != start_bb_bounds.end(); ++it) {
			BasicBlock *bb = it->first;
			O << "BB " << bb->getParent()->getNameStr() << "."
				<< bb->getNameStr() << "\n";
			print_bounds_in_bb(O, it->second);
		}
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

	void CaptureConstraints::declare_bounds_in_func(Function *f) {
		// We only handle function parameters and instructions for now. 
		// Function parameters.
		for (Function::arg_iterator ai = f->arg_begin();
				ai != f->arg_end(); ++ai) {
			if (!isa<IntegerType>(ai->getType()))
				continue;
			// A function parameter dominates all BBs.
			for (Function::iterator bi = f->begin(); bi != f->end(); ++bi) {
				start_bb_bounds[bi][ai] = make_pair(
						create_new_symbol(),
						create_new_symbol());
			}
		}
		// Instructions
		DominatorTree &DT = getAnalysis<DominatorTree>(*f);
		for (Function::iterator bi = f->begin(); bi != f->end(); ++bi) {
			BBList dominated;
			for (Function::iterator bi2 = f->begin(); bi2 != f->end(); ++bi2) {
				if (DT.dominates(bi, bi2))
					dominated.push_back(bi2);
			}
			for (BasicBlock::iterator ii = bi->begin(); ii != bi->end(); ++ii) {
				if (!isa<IntegerType>(ii->getType()))
					continue;
				forall(BBList, it, dominated) {
					BasicBlock *dom = *it;
					if (dom != bi || isa<PHINode>(ii)) {
						start_bb_bounds[dom][ii] = make_pair(
								create_new_symbol(),
								create_new_symbol());
					}
				}
			}
		}
	}

	CaptureConstraints::Expr CaptureConstraints::create_new_symbol() {
		Expr res = get_symbol_name(n_symbols);
		n_symbols++;
		return res;
	}

	void CaptureConstraints::compute_bound(
			User *user, BasicBlock *bb,
			ValueBoundsInBB &end_bb_bounds) {
		assert(user->getNumOperands() == 1 || user->getNumOperands() == 2);
		Value *op1 = (user->getNumOperands() >= 1 ? user->getOperand(0) : NULL);
		Value *op2 = (user->getNumOperands() >= 2 ? user->getOperand(1) : NULL);
		// Get opcode
		unsigned opcode;
		if (isa<Instruction>(user))
			opcode = dyn_cast<Instruction>(user)->getOpcode();
		else if (isa<ConstantExpr>(user))
			opcode = dyn_cast<ConstantExpr>(user)->getOpcode();
		else
			assert(false);
		// Big switch.
		if (opcode == Instruction::Add) {
			end_bb_bounds[user].first = create_expr(
					"+",
					get_lower_bound(op1, end_bb_bounds),
					get_lower_bound(op2, end_bb_bounds));
			end_bb_bounds[user].second = create_expr(
					"+",
					get_upper_bound(op1, end_bb_bounds),
					get_upper_bound(op2, end_bb_bounds));
		} else if (opcode == Instruction::Sub) {
			end_bb_bounds[user].first = create_expr(
					"-",
					get_lower_bound(op1, end_bb_bounds),
					get_upper_bound(op2, end_bb_bounds));
			end_bb_bounds[user].second = create_expr(
					"-",
					get_upper_bound(op1, end_bb_bounds),
					get_lower_bound(op2, end_bb_bounds));
		} else if (opcode == Instruction::Mul) {
			end_bb_bounds[user].first = create_expr(
					"*",
					get_lower_bound(op1, end_bb_bounds),
					get_lower_bound(op2, end_bb_bounds));
			end_bb_bounds[user].second = create_expr(
					"*",
					get_upper_bound(op1, end_bb_bounds),
					get_upper_bound(op2, end_bb_bounds));
		} else if (opcode == Instruction::UDiv || opcode == Instruction::SDiv) {
			end_bb_bounds[user].first = create_expr(
					"/",
					get_lower_bound(op1, end_bb_bounds),
					get_upper_bound(op2, end_bb_bounds));
			end_bb_bounds[user].second = create_expr(
					"/",
					get_upper_bound(op1, end_bb_bounds),
					get_lower_bound(op2, end_bb_bounds));
		} else if (opcode == Instruction::ZExt || opcode == Instruction::SExt) {
			end_bb_bounds[user].first = get_lower_bound(op1, end_bb_bounds);
			end_bb_bounds[user].second = get_upper_bound(op1, end_bb_bounds);
		} else {
			end_bb_bounds[user] = make_pair(get_infty_small(), get_infty_large());
		}
	}


	CaptureConstraints::Expr CaptureConstraints::get_lower_bound(
			Value *v, const ValueBoundsInBB &end_bb_bounds) {
		assert(end_bb_bounds.count(v));
		return end_bb_bounds.lookup(v).first;
	}

	CaptureConstraints::Expr CaptureConstraints::get_upper_bound(
			Value *v, const ValueBoundsInBB &end_bb_bounds) {
		assert(end_bb_bounds.count(v));
		return end_bb_bounds.lookup(v).second;
	}

	CaptureConstraints::Expr CaptureConstraints::get_const_expr(
			ConstantInt *v) {
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

	CaptureConstraints::Expr CaptureConstraints::get_infty_large() {
		return "infty";
	}

	CaptureConstraints::Expr CaptureConstraints::get_infty_small() {
		return "-infty";
	}

	CaptureConstraints::Expr CaptureConstraints::create_expr(
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
			case Instruction::ZExt:
			case Instruction::SExt:
				return true;
			default:
				return false;
		}
	}

	void CaptureConstraints::gen_bounds(
			Value *v, BasicBlock *bb,
			ValueBoundsInBB &end_bb_bounds) {
		assert(v);
		cerr << "gen_bounds:";
		v->dump();

		// Skip if not an integer.
		if (!isa<IntegerType>(v->getType()))
			return;
		// Skip if already computed. 
		if (end_bb_bounds.count(v))
			return;
		// Handle constant integers (leaves of the expression tree). 
		if (isa<ConstantInt>(v)) {
			ConstantInt *c = dyn_cast<ConstantInt>(v);
			end_bb_bounds[v] = make_pair(get_const_expr(c), get_const_expr(c));
			return;
		}
		// Get opcode
		unsigned opcode;
		if (isa<Instruction>(v))
			opcode = dyn_cast<Instruction>(v)->getOpcode();
		else if (isa<ConstantExpr>(v))
			opcode = dyn_cast<ConstantExpr>(v)->getOpcode();
		else
			assert(false);
		// Skip non-integer-arithmetic operations. 
		if (!is_int_operation(opcode)) {
			end_bb_bounds[v] = make_pair(get_infty_small(), get_infty_large());
			return;
		}
		// Either instruction or constant expr.
		User *u = dyn_cast<User>(v);
		assert(u);
		// All operands are integer?
		bool all_oprs_int = true;
		for (User::op_iterator oi = u->op_begin();
				oi != u->op_end(); ++oi) {
			Value *opr = *oi;
			if (!isa<IntegerType>(opr->getType())) {
				all_oprs_int = false;
				break;
			}
		}
		// If some operands are not integer, we cannot infer anything for now. 
		if (!all_oprs_int) {
			end_bb_bounds[u] = make_pair(get_infty_small(), get_infty_large());
			return;
		}
		// Recursively look at each operand to handle ConstantExpr. 
		for (User::op_iterator oi = u->op_begin();
				oi != u->op_end(); ++oi) {
			Value *opr = *oi;
			gen_bounds(opr, bb, end_bb_bounds);
		}
		// The bound of each operand has been calculated. 
		compute_bound(u, bb, end_bb_bounds);
	}

	void CaptureConstraints::calc_end_bb_bounds(
			BasicBlock *bb,
			ValueBoundsInBB &end_bb_bounds) {
		// Start-of-bb bounds are also part of the end-of-bb bounds. 
		end_bb_bounds = start_bb_bounds[bb];
		// Iterate through each instruction in the BB. 
		forall(BasicBlock, ii, *bb) {
			// Handle PHI nodes later. 
			if (isa<PHINode>(ii))
				continue;
			// Collect constraints from instruction <ii>.
			gen_bounds(ii, bb, end_bb_bounds);
		}
	}

	void CaptureConstraints::capture_in_func(Function *f) {
		// Declare all lower bounds and upper bounds. 
		declare_bounds_in_func(f);
		// Compute end-of-bb bounds.
		forall(Function, bi, *f) {
			cerr << "Compute end-of-bb bounds of "
				<< bi->getParent()->getNameStr() << "."
				<< bi->getNameStr() << "\n";
			ValueBoundsInBB end_bb_bounds;
			calc_end_bb_bounds(bi, end_bb_bounds);
			print_bounds_in_bb(errs(), end_bb_bounds);
		}
	}

	bool CaptureConstraints::runOnModule(Module &M) {
		forallfunc(M, fi) {
			if (!fi->isDeclaration())
				capture_in_func(fi);
		}
		return false;
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

