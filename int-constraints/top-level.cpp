#include "llvm/Support/CFG.h"
#include "llvm/Analysis/Dominators.h"
#include "common/include/util.h"
#include "common/include/typedefs.h"
using namespace llvm;

#include "config.h"
#include "capture.h"

namespace slicer {

#if 0
	void CaptureConstraints::print_bounds_in_bb(
			raw_ostream &O, const ValueBoundsInBB &bounds) {
		forallconst(ValueBoundsInBB, it, bounds) {
			O << "\t";
			it->first->print(O);
			O << "\n";
			O << "\t[" << it->second.first << ", " << it->second.second << "]\n";
		}
	}

	void CaptureConstraints::capture_in_func(Function *f) {
		// Declare all lower bounds and upper bounds. 
		declare_bounds_in_func(f);
		// Compute end-of-bb bounds.
		forall(Function, bi, *f) {
#ifdef VERBOSE
			cerr << "Compute end-of-bb bounds of "
				<< bi->getParent()->getNameStr() << "."
				<< bi->getNameStr() << "\n";
#endif
			ValueBoundsInBB end_bb_bounds;
			calc_end_bb_bounds(bi, end_bb_bounds);
#ifdef VERBOSE
			print_bounds_in_bb(errs(), end_bb_bounds);
#endif
			for (succ_iterator si = succ_begin(bi); si != succ_end(bi); ++si)
				collect_inter_bb_constraints(bi, *si, end_bb_bounds);
		}
	}

	void CaptureConstraints::collect_branch_constraints(
			Value *v1, Value *v2, CmpInst::Predicate pred,
			const ValueBoundsInBB &end_bb_bounds,
			vector<BoundsInBB> &branch_bounds) {
		if (!CmpInst::isIntPredicate(pred))
			return;
		// Only process ICmpInst, therefore <v1> and <v2> must be integers. 
		assert(isa<IntegerType>(v1->getType()) && isa<IntegerType>(v2->getType()));
		assert(end_bb_bounds.count(v1) && end_bb_bounds.count(v2));
		switch (pred) {
			case CmpInst::ICMP_SLE:
			case CmpInst::ICMP_ULE:
				// v1 <= v2
				branch_bounds.push_back(make_pair(v1, make_pair(
								get_lower_bound(v1, end_bb_bounds),
								get_upper_bound(v2, end_bb_bounds))));
				branch_bounds.push_back(make_pair(v2, make_pair(
								get_lower_bound(v1, end_bb_bounds),
								get_upper_bound(v2, end_bb_bounds))));
				break;
			case CmpInst::ICMP_SGE:
			case CmpInst::ICMP_UGE:
				// v1 >= v2
				branch_bounds.push_back(make_pair(v1, make_pair(
								get_lower_bound(v2, end_bb_bounds),
								get_upper_bound(v1, end_bb_bounds))));
				branch_bounds.push_back(make_pair(v2, make_pair(
								get_lower_bound(v2, end_bb_bounds),
								get_upper_bound(v1, end_bb_bounds))));
				break;
			case CmpInst::ICMP_SLT:
			case CmpInst::ICMP_ULT:
				// v1 < v2
				branch_bounds.push_back(make_pair(v1, make_pair(
								get_lower_bound(v1, end_bb_bounds),
								create_expr("-", get_upper_bound(v2, end_bb_bounds), "1"))));
				branch_bounds.push_back(make_pair(v2, make_pair(
								create_expr("+", get_lower_bound(v1, end_bb_bounds), "1"),
								get_upper_bound(v2, end_bb_bounds))));
				break;
			case CmpInst::ICMP_SGT:
			case CmpInst::ICMP_UGT:
				// v1 > v2
				branch_bounds.push_back(make_pair(v1, make_pair(
								create_expr("+", get_lower_bound(v2, end_bb_bounds), "1"),
								get_upper_bound(v1, end_bb_bounds))));
				branch_bounds.push_back(make_pair(v2, make_pair(
								get_lower_bound(v2, end_bb_bounds),
								create_expr("-", get_upper_bound(v1, end_bb_bounds), "1"))));
				break;
			case CmpInst::ICMP_EQ:
				// v1 == v2
				branch_bounds.push_back(make_pair(v1, make_pair(
								get_lower_bound(v2, end_bb_bounds),
								get_upper_bound(v2, end_bb_bounds))));
				branch_bounds.push_back(make_pair(v2, make_pair(
								get_lower_bound(v1, end_bb_bounds),
								get_lower_bound(v1, end_bb_bounds))));
				break;
			case CmpInst::ICMP_NE:
				// v1 != v2 <==> v1 < v2 || v1 > v2
				// v1 < v2
				branch_bounds.push_back(make_pair(v1, make_pair(
								get_lower_bound(v1, end_bb_bounds),
								create_expr("-", get_upper_bound(v2, end_bb_bounds), "1"))));
				branch_bounds.push_back(make_pair(v2, make_pair(
								create_expr("+", get_lower_bound(v1, end_bb_bounds), "1"),
								get_upper_bound(v2, end_bb_bounds))));
				// v1 > v2
				branch_bounds.push_back(make_pair(v1, make_pair(
								create_expr("+", get_lower_bound(v2, end_bb_bounds), "1"),
								get_upper_bound(v1, end_bb_bounds))));
				branch_bounds.push_back(make_pair(v2, make_pair(
								get_lower_bound(v2, end_bb_bounds),
								create_expr("-", get_upper_bound(v1, end_bb_bounds), "1"))));
				break;
			default: /* Do nothing */ ;
		}
	}

	void CaptureConstraints::collect_inter_bb_constraints(
			BasicBlock *x, BasicBlock *y, const ValueBoundsInBB &end_bb_bounds) {
		// Need to consider the branch instructions. 
		vector<BoundsInBB> branch_bounds;
		BranchInst *bi = dyn_cast<BranchInst>(x->getTerminator());
		if (bi && bi->isConditional()) {
			// We need to include the branch condition as well. 
			assert(bi->getNumSuccessors() == 2);
			ICmpInst *cond = dyn_cast<ICmpInst>(bi->getCondition());
			if (cond) {
				assert(cond->getNumOperands() == 2);
				Value *v1 = cond->getOperand(0), *v2 = cond->getOperand(1);
				CmpInst::Predicate pred = cond->getPredicate();
				// Negate the predicate if <y> is the false branch. 
#if 0
				// TODO: hack
				if (pred == CmpInst::ICMP_EQ)
					pred = CmpInst::ICMP_SGE;
#endif
				if (y == bi->getSuccessor(1))
					pred = CmpInst::getInversePredicate(pred);
				collect_branch_constraints(v1, v2, pred, end_bb_bounds, branch_bounds);
			}
		}
		// Link the variables used in both BBs. 
		ValueBoundsInBB &bounds_y = start_bb_bounds[y];
		forall(ValueBoundsInBB, it, bounds_y) {
			link_edge(x, it->first, y, it->first,
					end_bb_bounds, branch_bounds, start_bb_bounds[y]);
		}
		// Handle PHI nodes. 
		for (BasicBlock::iterator ii = y->begin();
				ii != (BasicBlock::iterator)(y->getFirstNonPHI()); ++ii) {
			// in BB <y>.
			PHINode *phi = dyn_cast<PHINode>(ii);
			assert(phi);
			for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
				if (phi->getIncomingBlock(i) == x) {
					Value *vx = phi->getIncomingValue(i);
					// <vx>/<ii> may not be even an integer.
					// Function <link_edge> will check that. 
					link_edge(x, vx, y, ii,
							end_bb_bounds, branch_bounds, start_bb_bounds[y]);
				}
			}
		}
	}

	void CaptureConstraints::link_edge(
			BasicBlock *x, Value *vx, BasicBlock *y, Value *vy,
			const ValueBoundsInBB &end_bb_bounds,
			const vector<BoundsInBB> &branch_bounds,
			const ValueBoundsInBB &bound_y) {
		
		if (!bound_y.count(vy))
			return;
		assert(!isa<Constant>(vy));
		// in BB <y>
		Expr lb = bound_y.lookup(vy).first, ub = bound_y.lookup(vy).second;

		vx->dump();
		if (isa<Constant>(vx)) {
			ValueBoundsInBB tmp_bounds;
			gen_bounds(vx, tmp_bounds);
			assert(tmp_bounds.count(vx));
			constraints.push_back(Clause::create_bool_expr(
						BoolExpr(lb, tmp_bounds[vx].first)));
			constraints.push_back(Clause::create_bool_expr(
						BoolExpr(tmp_bounds[vx].second, ub)));
			return;
		}

		if (!end_bb_bounds.count(vx))
			return;

		Clause *root = NULL;
		for (size_t j = 0; j < branch_bounds.size(); ++j) {
			if (branch_bounds[j].first == vx) {
				Clause *cur = Clause::create_and(
						Clause::create_or(
							Clause::create_bool_expr(
								BoolExpr(lb, end_bb_bounds.lookup(vx).first)),
							Clause::create_bool_expr(
								BoolExpr(lb, branch_bounds[j].second.first))),
						Clause::create_or(
							Clause::create_bool_expr(
								BoolExpr(end_bb_bounds.lookup(vx).second, ub)),
							Clause::create_bool_expr(
								BoolExpr(branch_bounds[j].second.second, ub))));
				if (root == NULL)
					root = cur;
				else
					root = Clause::create_or(root, cur);
			}
		}
		if (root)
			constraints.push_back(root);
		else {
			constraints.push_back(Clause::create_bool_expr(
						BoolExpr(lb, end_bb_bounds.lookup(vx).first)));
			constraints.push_back(Clause::create_bool_expr(
						BoolExpr(end_bb_bounds.lookup(vx).second, ub)));
		}
	}

	void CaptureConstraints::calc_end_bb_bounds(
			BasicBlock *bb,
			ValueBoundsInBB &end_bb_bounds) {
		// Start-of-bb bounds are also part of the end-of-bb bounds. 
		end_bb_bounds = start_bb_bounds[bb];
		// Iterate through each instruction in the BB. 
		forall(BasicBlock, ii, *bb) {
			// We'll handle PHI nodes later. 
			if (!isa<PHINode>(ii)) {
				// Collect constraints from instruction <ii>.
				gen_bounds(ii, end_bb_bounds);
			}
		}
	}

	void CaptureConstraints::gen_bounds(
			Value *v, ValueBoundsInBB &end_bb_bounds) {
		assert(v);
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
			gen_bounds(opr, end_bb_bounds);
		}
		// The bound of each operand has been calculated. 
		gen_bound_of_user(u, end_bb_bounds);
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
				Expr lb = create_new_symbol();
				Expr ub = create_new_symbol();
				start_bb_bounds[bi][ai] = make_pair(lb, ub);
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
						Expr lb = create_new_symbol();
						Expr ub = create_new_symbol();
						start_bb_bounds[dom][ii] = make_pair(lb, ub);
					}
				}
			}
		}
	}

	void CaptureConstraints::gen_bound_of_user(
			User *user, ValueBoundsInBB &end_bb_bounds) {
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
		} else if (opcode == Instruction::ICmp) {
			end_bb_bounds[user] = make_pair("0", "1");
		} else {
			end_bb_bounds[user] = make_pair(get_infty_small(), get_infty_large());
		}
	}
#endif
}
