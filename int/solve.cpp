/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "int"

// TODO: Put into the configuration file
// #define CHECK_BOUND
#define CHECK_DIV

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Analysis/Dominators.h"
#include "common/include/util.h"
#include "common/cfg/intra-reach.h"
using namespace llvm;

#include <list>
#include <iostream>
#include <sstream>
using namespace std;

#include "capture.h"
#include "solve.h"
#include "config.h"
using namespace slicer;

static RegisterPass<SolveConstraints> X(
		"solve",
		"Solve captured constraints using STP",
		false,
		true); // is analysis

char SolveConstraints::ID = 0;

VC SolveConstraints::vc = NULL;
sys::Mutex SolveConstraints::vc_mutex(false); // not recursive
DenseMap<string, VCExpr> SolveConstraints::symbols;

void SolveConstraints::destroy_vc() {
	assert(vc && "create_vc and destroy_vc are not paired");
	for (DenseMap<string, VCExpr>::iterator it = symbols.begin();
			it != symbols.end(); ++it)
		vc_DeleteExpr(it->second);
	symbols.clear();
	vc_Destroy(vc);
	vc = NULL;
	vc_mutex.release();
}

void SolveConstraints::create_vc() {
	assert(vc_mutex.tryacquire() && "There can be only one VC instance running");
	assert(!vc && "create_vc and destroy_vc are not paired");
	vc = vc_createValidityChecker();
	// Don't delete persistant nodes on vc_Destroy. 
	// We are responsible to delete them. 
	vc_setInterfaceFlags(vc, EXPRDELETE, 0);
	vc_registerErrorHandler(vc_error_handler);
	assert(symbols.empty());
	assert(vc && "Failed to create a VC");
}

void SolveConstraints::releaseMemory() {
	// Principally paired with the create_vc in runOnModule. 
	destroy_vc();
}

bool SolveConstraints::runOnModule(Module &M) {
	// Principally paired with the destroy_vc in releaseMemory.
	create_vc();
	calculate(M);
	return false;
}

void SolveConstraints::recalculate(Module &M) {
	calculate(M);
}

void SolveConstraints::calculate(Module &M) {

	// Reinitialize the STP solver. 
	destroy_vc();
	create_vc();

	root.clear();
	identify_eqs(); // This step does not require <vc>.
	translate_captured(M);
}

void SolveConstraints::identify_eqs() {

	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	for (unsigned i = 0; i < CC.get_num_constraints(); ++i) {
		const Clause *c = CC.get_constraint(i);
		const Value *v1 = NULL, *v2 = NULL;
		if (is_simple_eq(c, &v1, &v2)) {
			assert(v1 && v2);
			const Value *r1 = get_root(v1), *r2 = get_root(v2);
			/*
			 * Make sure constant integers will always be the roots. 
			 * Otherwise, we would miss information when replacing
			 * a variable with roots. 
			 */
			if (isa<ConstantInt>(r1) || isa<ConstantPointerNull>(r1))
				root[r2] = r1;
			else
				root[r1] = r2;
		}
	}
}

void SolveConstraints::refine_candidates(list<const Value *> &candidates) {

	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();

	list<const Value *>::iterator i, j, to_del;
	ConstValueSet tree_contains_consts;
	const ConstValueSet &integers = CC.get_integers();
	forallconst(ConstValueSet, it, integers) {
		const Value *v = *it;
		if (CC.is_constant_integer(v))
			tree_contains_consts.insert(get_root(v));
	}

	for (i = candidates.begin(); i != candidates.end(); ) {
		const Value *v = *i;
		bool to_be_removed = false;
		// Only try fixing the roots of equivalent classes. 
		to_be_removed = to_be_removed || (get_root(v) != v);
		// NOTE: <v> represents its equivalent class rather than just itself. 
		// Only try those integers defined only once. 
		to_be_removed = to_be_removed || !tree_contains_consts.count(v);
		// Don't try fixing a pointer. TODO: ConstantPointerNULL. 
		to_be_removed = to_be_removed || (!isa<IntegerType>(v->getType()));
		// Don't try fixing an alreayd fixed value. 
		to_be_removed = to_be_removed || isa<ConstantInt>(v);
		if (!to_be_removed) {
			++i;
		} else {
			to_del = i;
			++i;
			candidates.erase(to_del);
		}
	}

	/*
	 * OPT: Many variables, although identified as constants (i.e. never changes
	 * in an execution), don't appear in any clause. These variables' values
	 * cannot be restricted. 
	 * TODO: More opt: Remove variables not connected with any ConstantInt. 
	 */
	ConstValueSet appeared;
	for (unsigned k = 0; k < CC.get_num_constraints(); ++k) {

		Clause *c = CC.get_constraint(k)->clone();
#if 0
		print_clause(errs(), c, getAnalysis<IDAssigner>());
		errs() << "\n";
#endif
		replace_with_root(c);

		vc_push(vc);
		VCExpr vce = translate_to_vc(c);
		VCExpr simplified_vce = vc_simplify(vc, vce);
		int ret = vc_isBool(simplified_vce);
		if (ret == 0) {
			errs() << "After replace: ";
			print_clause(errs(), c, getAnalysis<IDAssigner>());
			errs() << "\n";
		}
		assert(ret != 0);
		// If can be proved by simplification, don't add it to the constraint set. 
		if (ret == -1)
			update_appeared(appeared, c);
		delete_vcexpr(vce);
		delete_vcexpr(simplified_vce);
		vc_pop(vc);

		delete c; // c is cloned
	}
	// Remove variables that don't appear in any clause. 
	for (i = candidates.begin(); i != candidates.end(); ) {
		if (appeared.count(*i))
			++i;
		else {
			to_del = i;
			++i;
			candidates.erase(to_del);
		}
	}
}

/* TODO: Could do the same thing for uses as well, but too many uses. */
void SolveConstraints::identify_fixed_values() {

	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();

	// Get all integers that are possible to be a fixed value. 
	// No need to be sound. 
	list<const Value *> candidates;
	const ConstValueSet &integers = CC.get_integers();
	candidates.insert(candidates.end(), integers.begin(), integers.end());
	refine_candidates(candidates);
	dbgs() << "# of candidates = " << candidates.size() << "\n";
	
	/*
	 * Try to constantize as many variables as possible. 
	 * Algorithm:
	 * 1. Find a satisfiable assignment, and assume each variable must equal
	 *    the assigned value. 
	 * 2. Try negating any one of them.
	 * 3. If the assignment is still satisfiable, this negated variable is 
	 *    not really fixed. 
	 * 4. Check each variable in the new assignment, and remove all variables
	 *    that have a new value. 
	 * 5. Return to Step 2. 
	 */
	// Find a satisfiable assignment. 
	list<pair<const Value *, pair<unsigned, int> > > fixed_values;
	list<pair<const Value *, pair<unsigned, int> > >::iterator i, j, to_del;
	
	vc_push(vc);
	dbgs() << "Constructing a satisfying assignment... ";
	VCExpr f = vc_falseExpr(vc);
	int ret = vc_query(vc, f);
	// TODO: diagnose if ret != 0. 
	assert(ret == 0);
	delete_vcexpr(f);
	dbgs() << "Done\n";
	
	forall(list<const Value *>, it, candidates) {
		const Value *v = *it;
		VCExpr vce = translate_to_vc(v);
		VCExpr ce = vc_getCounterExample(vc, vce);
		fixed_values.push_back(make_pair(
					v, make_pair(getBVUnsigned(ce), vc_getBVLength(vc, ce))));
		delete_vcexpr(vce);
		delete_vcexpr(ce);
	}
	vc_pop(vc);
	
	// Try proving each guess. 
	unsigned n_fixed = 0, n_not_fixed = 0, n_opted = 0;
	for (i = fixed_values.begin(); i != fixed_values.end(); ) {

		vc_push(vc);
		VCExpr guessed_value = vc_bvConstExprFromInt(vc,
				i->second.second, i->second.first);
		VCExpr vce = translate_to_vc(i->first);
		VCExpr eq = vc_eqExpr(vc, vce, guessed_value);
		int fixed = vc_query(vc, eq);
		assert(fixed != 2);
		if (fixed == 1) {
			// <i>'s value must be fixed. Skip to the next candidate. 
			// FIXME: dbgs() does not support colors? 
			errs().changeColor(raw_ostream::GREEN) << "Y"; errs().resetColor();
			++n_fixed;
#if 0
			Expr *e = new Expr(i->first);
			print_expr(errs(), e, getAnalysis<IDAssigner>());
			delete e;
			errs() << " = " << i->second.first;
			errs() << " is fixed: " << *(i->first) << "\n";
#endif
			++i;
		} else {
			errs().changeColor(raw_ostream::RED) << "N"; errs().resetColor();
			++n_not_fixed;
#if 0
			Expr *e = new Expr(i->first);
			print_expr(errs(), e, getAnalysis<IDAssigner>());
			delete e;
			errs() << " is NOT fixed: " << *(i->first) << "\n";
#endif
			j = i; ++j;
			while (j != fixed_values.end()) {
				VCExpr vj = translate_to_vc(j->first);
				VCExpr ce = vc_getCounterExample(vc, vj);
				if (j->second.first == getBVUnsigned(ce)) {
					++j;
				} else {
					to_del = j;
					++j;
					errs().changeColor(raw_ostream::BLUE) << "O"; errs().resetColor();
					++n_opted;
#if 0
					errs() << "optimized: ";
					Expr *ex = new Expr(to_del->first);
					print_expr(errs(), ex, getAnalysis<IDAssigner>());
					delete ex;
					errs() << "\n";
#endif
					fixed_values.erase(to_del);
				}
				delete_vcexpr(vj);
				delete_vcexpr(ce);
			}
			// <i> does not have a fixed value. 
			to_del = i;
			++i;
			fixed_values.erase(to_del);
		}

		delete_vcexpr(guessed_value);
		delete_vcexpr(vce);
		delete_vcexpr(eq);
		vc_pop(vc);
	}
	
	dbgs() << "\n";
	dbgs() << "fixed = " << n_fixed << "; not fixed = " << n_not_fixed <<
		"; opted = " << n_opted << "\n";
	// Finally, make identified fixed values to be roots. 
	for (i = fixed_values.begin(); i != fixed_values.end(); ++i) {
		const Value *v = i->first;
		root[v] = ConstantInt::get(v->getType(), i->second.first);
	}
}

void SolveConstraints::update_appeared(
		ConstValueSet &appeared, const Clause *c) {
	if (c->be)
		update_appeared(appeared, c->be);
	else {
		update_appeared(appeared, c->c1);
		update_appeared(appeared, c->c2);
	}
}

void SolveConstraints::update_appeared(
		ConstValueSet &appeared, const BoolExpr *be) {
	update_appeared(appeared, be->e1);
	update_appeared(appeared, be->e2);
}

void SolveConstraints::update_appeared(
		ConstValueSet &appeared, const Expr *e) {
	if (e->type == Expr::SingleDef)
		appeared.insert(e->v);
	else if (e->type == Expr::SingleUse)
		appeared.insert(e->u->get());
	else if (e->type == Expr::Unary)
		update_appeared(appeared, e->e1);
	else if (e->type == Expr::Binary) {
		update_appeared(appeared, e->e1);
		update_appeared(appeared, e->e2);
	} else
		assert_not_supported();
}

void SolveConstraints::diagnose(Module &M) {

	dbgs() << "Detected inconsistency. Enter diagnose mode...\n";

	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	unsigned n_constraints = CC.get_num_constraints();
	DenseSet<unsigned> does_not_matter;
	for (unsigned j = 0; j < n_constraints; ++j) {
		// Decide if <j> matters. 
		does_not_matter.insert(j);
		destroy_vc();
		create_vc();
		for (unsigned i = 0; i < n_constraints; ++i) {
			if (does_not_matter.count(i))
				continue;
			const Clause *c = CC.get_constraint(i);
			VCExpr vce = translate_to_vc(c);
			if (can_be_simplified(vce) == -1)
				vc_assertFormula(vc, vce);
			delete_vcexpr(vce);
		}
		VCExpr f = vc_falseExpr(vc);
		int ret = vc_query(vc, f);
		delete_vcexpr(f);
		if (ret == 0) {
			dbgs() << "Constraint " << j << " matters.\n";
			does_not_matter.erase(j);
		} else {
			dbgs() << "Constraint " << j << " does not matter.\n";
		}
	}

	dbgs() << "Start printing a minimal set...\n";
	for (unsigned i = 0; i < n_constraints; ++i) {
		if (!does_not_matter.count(i)) {
			print_clause(dbgs(), CC.get_constraint(i), getAnalysis<IDAssigner>());
			dbgs() << "\n";
		}
	}
	dbgs() << "Finished printing a minimal set\n";
}

void SolveConstraints::translate_captured(Module &M) {

	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();

	assert(vc);
	unsigned n_constraints = CC.get_num_constraints();
	dbgs() << "# of constraints = " << n_constraints << "\n";
	for (unsigned i = 0; i < n_constraints; ++i) {
		
		Clause *c = CC.get_constraint(i)->clone();
		replace_with_root(c);

		VCExpr vce = translate_to_vc(c);
		if (can_be_simplified(vce) == -1)
			vc_assertFormula(vc, vce);
		delete_vcexpr(vce);
		
		delete c; // c is cloned
	}
	
	// The captured constraints should be consistent. 
	vc_push(vc);
#if 0
	vc_assertFormula(vc, vc_sbvGtExpr(vc,
				vc_varExpr(vc, "x2293", vc_bv32Type(vc)),
				vc_zero(vc)));
#endif
#if 0
	Clause *c = CC.get_constraint(788)->clone();
	print_clause(errs(), c, getAnalysis<IDAssigner>());
	replace_with_root(c);
	vc_assertFormula(vc, translate_to_vc(c));
	delete c;
#endif
#if 0
	vc_assertFormula(vc, vc_eqExpr(vc,
				vc_varExpr(vc, "x2298", vc_bv32Type(vc)),
				vc_bv32PlusExpr(vc,
					vc_varExpr(vc, "x2265", vc_bv32Type(vc)),
					vc_varExpr(vc, "x2288", vc_bv32Type(vc)))));
#endif
#if 0
	vc_assertFormula(vc, vc_sbvGtExpr(vc,
				vc_varExpr(vc, "x2293", vc_bv32Type(vc)),
				vc_varExpr(vc, "x2032", vc_bv32Type(vc))));
#endif
#if 0
	vc_assertFormula(vc, vc_xorExpr(vc,
				vc_eqExpr(vc,
					vc_varExpr(vc, "x2307", vc_bvType(vc, 1)),
					vc_bvConstExprFromInt(vc, 1, 0)),
				vc_sbvLtExpr(vc,
					vc_varExpr(vc, "x2293", vc_bv32Type(vc)),
					vc_varExpr(vc, "x1939", vc_bv32Type(vc)))));
#endif
	dbgs() << "Checking consistency... ";
	if (print_asserts) {
		vc_printVarDecls(vc);
		vc_printAsserts(vc);
	}
	VCExpr f = vc_falseExpr(vc);
	int ret = vc_query(vc, f);
	delete_vcexpr(f);
	vc_pop(vc);

	if (ret != 0) {
		diagnose(M);
		// CC.print(errs(), &M);
	}
	assert(ret == 0 && "The captured constraints is inconsistent.");
	dbgs() << "Done\n";
}

int SolveConstraints::can_be_simplified(VCExpr e) {
	vc_push(vc);
	VCExpr simplified = vc_simplify(vc, e);
	int ret = vc_isBool(simplified);
	delete_vcexpr(simplified);
	vc_pop(vc);
	return ret;
}

ConstantInt *SolveConstraints::get_fixed_value(const Value *v) {
	const Value *root = get_root(v);
	if (const ConstantInt *ci = dyn_cast<ConstantInt>(root))
		return ConstantInt::get(ci->getContext(), ci->getValue());
	else if (isa<ConstantPointerNull>(root)) {
		return ConstantInt::get(IntegerType::get(root->getContext(), 32), 0);
	} else {
		return NULL;
	}
}

void SolveConstraints::replace_with_root(Clause *c) {
	if (c->be)
		replace_with_root(c->be);
	else {
		replace_with_root(c->c1);
		replace_with_root(c->c2);
	}
}

void SolveConstraints::replace_with_root(BoolExpr *be) {
	replace_with_root(be->e1);
	replace_with_root(be->e2);
}

void SolveConstraints::replace_with_root(Expr *e) {
	if (e->type == Expr::SingleDef || e->type == Expr::LoopBound) {
		e->v = get_root(e->v);
	} else if (e->type == Expr::SingleUse) {
		e->type = Expr::SingleDef;
		e->v = get_root(e->u->get());
	} else if (e->type == Expr::Unary) {
		replace_with_root(e->e1);
	} else if (e->type == Expr::Binary) {
		replace_with_root(e->e1);
		replace_with_root(e->e2);
	} else {
		assert_not_supported();
	}
}

const Value *SolveConstraints::get_root(const Value *x) {
	assert(x);
	if (!root.count(x))
		return x;
	const Value *y = root.lookup(x);
	if (y != x) {
		const Value *ry = get_root(y);
		root[x] = ry;
	}
	return root.lookup(x);
}

bool SolveConstraints::is_simple_eq(
		const Clause *c, const Value **v1, const Value **v2) {
	if (c->be == NULL)
		return false;
	if (c->be->p != CmpInst::ICMP_EQ)
		return false;
	// <is_simple_eq> ignores Expr::LoopBound. 
	if (c->be->e1->type != Expr::SingleDef || c->be->e2->type != Expr::SingleDef)
		return false;
	if (v1)
		*v1 = c->be->e1->v;
	if (v2)
		*v2 = c->be->e2->v;
	return true;
}

void SolveConstraints::vc_error_handler(const char *err_msg) {
	errs() << "Error in VC: ";
	errs() << err_msg << "\n";
}

VCExpr SolveConstraints::translate_to_vc(const Clause *c) {
	if (c->be)
		return translate_to_vc(c->be);
	VCExpr vce1 = translate_to_vc(c->c1);
	VCExpr vce2 = translate_to_vc(c->c2);
	VCExpr res;
	if (c->op == Instruction::And)
		res = vc_andExpr(vc, vce1, vce2);
	else if (c->op == Instruction::Or)
		res = vc_orExpr(vc, vce1, vce2);
	else
		res = vc_xorExpr(vc, vce1, vce2);
	delete_vcexpr(vce1);
	delete_vcexpr(vce2);
	return res;
}

VCExpr SolveConstraints::translate_to_vc(const BoolExpr *be) {
	const Expr *e1 = be->e1, *e2 = be->e2;
	assert(e1->get_width() == e2->get_width());
	VCExpr vce1 = translate_to_vc(e1);
	VCExpr vce2 = translate_to_vc(e2);
	VCExpr res;
	switch (be->p) {
		case CmpInst::ICMP_EQ:
			res = vc_eqExpr(vc, vce1, vce2);
			break;
		case CmpInst::ICMP_NE:
			{
				VCExpr eq = vc_eqExpr(vc, vce1, vce2);
				res = vc_notExpr(vc, eq);
				delete_vcexpr(eq);
			}
			break;
		case CmpInst::ICMP_UGT:
			res = vc_bvGtExpr(vc, vce1, vce2);
			break;
		case CmpInst::ICMP_SGT:
			res = vc_sbvGtExpr(vc, vce1, vce2);
			break;
		case CmpInst::ICMP_UGE:
			res = vc_bvGeExpr(vc, vce1, vce2);
			break;
		case CmpInst::ICMP_SGE:
			res = vc_sbvGeExpr(vc, vce1, vce2);
			break;
		case CmpInst::ICMP_ULT:
			res = vc_bvLtExpr(vc, vce1, vce2);
			break;
		case CmpInst::ICMP_SLT:
			res = vc_sbvLtExpr(vc, vce1, vce2);
			break;
		case CmpInst::ICMP_ULE:
			res = vc_bvLeExpr(vc, vce1, vce2);
			break;
		case CmpInst::ICMP_SLE:
			res = vc_sbvLeExpr(vc, vce1, vce2);
			break;
		default: assert(false && "Invalid predicate");
	}
	delete_vcexpr(vce1);
	delete_vcexpr(vce2);
	return res;
}

VCExpr SolveConstraints::translate_to_vc(const Expr *e) {
	if (e->type == Expr::SingleDef)
		return translate_to_vc(e->v);
	if (e->type == Expr::LoopBound)
		return translate_to_vc(e->v, true);
	if (e->type == Expr::SingleUse)
		return translate_to_vc(e->u);
	if (e->type == Expr::Unary) {
		VCExpr child = translate_to_vc(e->e1);
		VCExpr res;
		switch (e->op) {
			case Instruction::SExt:
				assert(e->e1->get_width() == 1);
				res = vc_bvSignExtend(vc, child, 32);
				break;
			case Instruction::ZExt:
				{
					assert(e->e1->get_width() == 1);
					// STP does not have bvUnsignExtend
					VCExpr zero_31 = vc_bvConstExprFromInt(vc, 31, 0);
					res = vc_bvConcatExpr(vc, zero_31, child);
					delete_vcexpr(zero_31);
				}
				break;
			case Instruction::Trunc:
				assert(e->e1->get_width() == 32);
				res = vc_bvExtract(vc, child, 0, 0);
				break;
			default: assert_not_supported();
		}
		delete_vcexpr(child);
		return res;
	}
	if (e->type == Expr::Binary) {
		VCExpr left = translate_to_vc(e->e1);
		VCExpr right = translate_to_vc(e->e2);
		avoid_overflow(e->op, left, right);
		VCExpr res;
		switch (e->op) {
			case Instruction::Add:
				res = vc_bv32PlusExpr(vc, left, right);
				break;
			case Instruction::Sub:
				res = vc_bv32MinusExpr(vc, left, right);
				break;
			case Instruction::Mul:
				res = vc_bv32MultExpr(vc, left, right);
				break;
			case Instruction::UDiv:
			case Instruction::SDiv:
				res = vc_sbvDivExpr(vc, 32, left, right);
				break;
			case Instruction::URem:
			case Instruction::SRem:
				res = vc_sbvModExpr(vc, 32, left, right);
				break;
			case Instruction::Shl:
				// left << right
				if (getExprKind(right) == BVCONST)
					res = vc_bv32LeftShiftExpr(vc, getBVUnsigned(right), left);
				else
					res = vc_bvVar32LeftShiftExpr(vc, right, left);
				break;
			case Instruction::LShr:
			case Instruction::AShr:
				// left >> right
				if (getExprKind(right) == BVCONST)
					res = vc_bv32RightShiftExpr(vc, getBVUnsigned(right), left);
				else
					res = vc_bvVar32RightShiftExpr(vc, right, left);
				break;
			case Instruction::And:
				res = vc_bvAndExpr(vc, left, right);
				break;
			case Instruction::Or:
				res = vc_bvOrExpr(vc, left, right);
				break;
			case Instruction::Xor:
				res = vc_bvXorExpr(vc, left, right);
				break;
			default: assert_not_supported();
		}
		delete_vcexpr(left);
		delete_vcexpr(right);
		return res;
	}
	assert(false && "Invalid expression type");
}

VCExpr SolveConstraints::translate_to_vc(const Value *v, bool is_loop_bound) {
	if (const ConstantInt *ci = dyn_cast<ConstantInt>(v)) {
		if (ci->getType()->getBitWidth() == 1) {
			VCExpr b = (ci->isOne() ? vc_trueExpr(vc) : vc_falseExpr(vc));
			VCExpr res = vc_boolToBVExpr(vc, b);
			delete_vcexpr(b);
			return res;
		} else {
			// TODO: Add warnings on very large constants.
			return vc_bv32ConstExprFromInt(vc, ci->getSExtValue());
		}
	}
	if (isa<ConstantPointerNull>(v)) {
		// null == 0
		return vc_zero(vc);
	}

	IDAssigner &IDA = getAnalysis<IDAssigner>();
	unsigned value_id = IDA.getValueID(v);
	assert(value_id != IDAssigner::INVALID_ID);

	ostringstream oss;
	oss << (is_loop_bound ? "lb": "x") << value_id;
	string name = oss.str();
	VCExpr &symbol = symbols[name];
	if (symbol == NULL) {
		VCType vct = (v->getType()->isIntegerTy(1) ?
				vc_bvType(vc, 1) :
				vc_bv32Type(vc));
		symbol = vc_varExpr(vc, name.c_str(), vct);
		delete_vcexpr(vct);
	}

	return symbol;
}

VCExpr SolveConstraints::translate_to_vc(const Use *u) {
	return translate_to_vc(u->get());
}

void SolveConstraints::print(raw_ostream &O, const Module *M) const {
	// Don't know what to do. 
}

void SolveConstraints::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequiredTransitive<IDAssigner>();
	AU.addRequiredTransitive<LoopInfo>();
	AU.addRequiredTransitive<DominatorTree>();
	AU.addRequiredTransitive<IntraReach>();
	AU.addRequiredTransitive<CaptureConstraints>();
	ModulePass::getAnalysisUsage(AU);
}

bool SolveConstraints::satisfiable(const Clause *c) {

	DEBUG(dbgs() << "Satisfiable?: ";
			print_clause(dbgs(), c, getAnalysis<IDAssigner>());
			dbgs() << "\n";);

	Clause *c2 = c->clone();
	replace_with_root(c2);
	// OPT: if in the format of v0 == v0, then must be true. 
	const Value *v1 = NULL, *v2 = NULL;
	if (is_simple_eq(c2, &v1, &v2) && v1 == v2) {
		delete c2;
		return true;
	}

	vc_push(vc);
	realize(c);
	VCExpr vce = translate_to_vc(c2);
	delete c2;
	VCExpr not_vce = vc_notExpr(vc, vce);
	delete_vcexpr(vce);

	if (print_asserts) {
		vc_printVarDecls(vc);
		vc_printAsserts(vc);
	}

	int ret = vc_query(vc, not_vce);
	delete_vcexpr(not_vce);
	if (print_counterexample && ret == 0)
		vc_printCounterExample(vc);
	vc_pop(vc);
	
	assert(ret != 2);
	return ret == 0;
}

bool SolveConstraints::contains_only_ints(const Clause *c) {
	if (c->be)
		return contains_only_ints(c->be);
	return contains_only_ints(c->c1) && contains_only_ints(c->c2);
}

bool SolveConstraints::contains_only_ints(const BoolExpr *be) {
	return contains_only_ints(be->e1) && contains_only_ints(be->e2);
}

bool SolveConstraints::contains_only_ints(const Expr *e) {
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	if (e->type == Expr::SingleDef || e->type == Expr::LoopBound)
		return CC.is_integer(e->v) || isa<Constant>(e->v);
	if (e->type == Expr::SingleUse)
		return CC.is_integer(e->u->get()) || isa<Constant>(e->u->get());
	if (e->type == Expr::Unary)
		return contains_only_ints(e->e1);
	if (e->type == Expr::Binary)
		return contains_only_ints(e->e1) && contains_only_ints(e->e2);
	assert_not_supported();
}

bool SolveConstraints::provable(const Clause *c) {
	
	DEBUG(dbgs() << "Provable?: ";
			print_clause(dbgs(), c, getAnalysis<IDAssigner>());
			dbgs() << "\n";);

	if (!contains_only_ints(c))
		return false;
	
	Clause *c2 = c->clone();
	replace_with_root(c2);
	// OPT: If is in the format of v0 == v0, then must be true.
	// FIXME: This may not be right, the realized constraints may contain
	// conflicts. 
	const Value *v1 = NULL, *v2 = NULL;
	if (is_simple_eq(c2, &v1, &v2) && v1 == v2) {
		delete c2;
		return true;
	}

	vc_push(vc);
	realize(c);
	VCExpr vce = translate_to_vc(c2);
	delete c2;
	int ret = vc_query(vc, vce);
	delete_vcexpr(vce);
	if (print_counterexample && ret == 0)
		vc_printCounterExample(vc);
	vc_pop(vc);

	assert(ret != 2);
	return ret == 1;
}

void SolveConstraints::realize(const Clause *c) {
	if (c->be)
		realize(c->be);
	else {
		realize(c->c1);
		realize(c->c2);
	}
}

void SolveConstraints::realize(const BoolExpr *be) {
	realize(be->e1);
	realize(be->e2);
}

void SolveConstraints::realize(const Expr *e) {
	if (e->type == Expr::Unary) {
		realize(e->e1);
	} else if (e->type == Expr::Binary) {
		realize(e->e1);
		realize(e->e2);
	} else if (e->type == Expr::SingleUse) {
		realize(e->u);
	} else {
		if (const Instruction *ins = dyn_cast<Instruction>(e->v))
			realize(ins);
	}
}

BasicBlock *SolveConstraints::get_idom(BasicBlock *bb) {
	DominatorTree &DT = getAnalysis<DominatorTree>(*bb->getParent());
	DomTreeNode *node = DT[bb];
	if (!node) {
		errs() << *bb << "\n";
	}
	assert(node);
	DomTreeNode *idom = node->getIDom();
	return (idom ? idom->getBlock() : NULL);
}

void SolveConstraints::realize(const Use *u) {
	// The value of a llvm::Constant is compile-time known. Therefore,
	// we don't need to capture extra constraints. 
	if (const Instruction *ins = dyn_cast<Instruction>(u->getUser()))
		realize(ins);
}

void SolveConstraints::realize(const Instruction *ins) {
	
	assert(ins);
	BasicBlock *bb = const_cast<BasicBlock *>(ins->getParent());
	Function *f = bb->getParent();

	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	// TODO: Make it inter-procedural
	IntraReach &IR = getAnalysis<IntraReach>(*f);
	LoopInfo &LI = getAnalysis<LoopInfo>(*f);

	// Realize each dominating BranchInst. 
	BasicBlock *dom = bb;
	while (dom != &f->getEntryBlock()) {
		BasicBlock *p = get_idom(dom);
		assert(p);
		/*
		 * If a successor of <p> cannot reach <dom>, the condition that leads
		 * to that successor must not hold. 
		 */ 
		TerminatorInst *ti = p->getTerminator();
		for (unsigned i = 0; i < ti->getNumSuccessors(); ++i) {
			if (!IR.reachable(ti->getSuccessor(i), dom)) {
				const Clause *c = CC.get_avoid_branch(ti, i);
				if (c) {
					DEBUG(dbgs() << "[realize] ";
							print_clause(dbgs(), c, getAnalysis<IDAssigner>());
							dbgs() << "\n";);
					Clause *c2 = c->clone();
					replace_with_root(c2);

					VCExpr vce = translate_to_vc(c2);
					if (can_be_simplified(vce) != 1)
						vc_assertFormula(vc, vce);
					delete_vcexpr(vce);

					delete c2;
					delete c;
				}
			}
		}
		dom = p;
	}

	// Realize each containing loop. 
	Loop *l = LI.getLoopFor(bb);
	while (l) {
		vector<Clause *> constraints_from_l;
		CC.get_loop_bound(l, constraints_from_l);
		for (size_t i = 0; i < constraints_from_l.size(); ++i) {
			Clause *c = constraints_from_l[i];
			DEBUG(dbgs() << "[realize] ";
					print_clause(dbgs(), c, getAnalysis<IDAssigner>());
					dbgs() << "\n";);
			Clause *c2 = c->clone();
			replace_with_root(c2);
			
			VCExpr vce = translate_to_vc(c2);
			if (can_be_simplified(vce) != 1)
				vc_assertFormula(vc, vce);
			delete_vcexpr(vce);

			delete c2;
		}
		for (size_t i = 0; i < constraints_from_l.size(); ++i)
			delete constraints_from_l[i];

		l = l->getParentLoop();
	}
}

void SolveConstraints::avoid_div_by_zero(VCExpr left, VCExpr right) {

	// TODO: We shouldn't assume the divisor > 0
	VCExpr zero = vc_zero(vc);
	VCExpr right_gt_0 = vc_sbvGtExpr(vc, right, zero);

	vc_assertFormula(vc, right_gt_0);

	delete_vcexpr(zero);
	delete_vcexpr(right_gt_0);
}

void SolveConstraints::avoid_overflow_shl(VCExpr left, VCExpr right) {

	// (left << right) <= oo ==> left <= (oo >> right)
	int bit_width = vc_getBVLength(vc, left);
	assert(vc_getBVLength(vc, right) == bit_width);

	VCExpr int_max = vc_int_max(vc);
	VCExpr left_ge_0 = vc_bvBoolExtract_Zero(vc, left, bit_width - 1);
	VCExpr int_max_shr = vc_bvVar32RightShiftExpr(vc, right, int_max);
	VCExpr left_le = vc_sbvLeExpr(vc, left, int_max_shr);

	vc_assertFormula(vc, left_ge_0);
	vc_assertFormula(vc, left_le);

	delete_vcexpr(int_max);
	delete_vcexpr(left_ge_0);
	delete_vcexpr(int_max_shr);
	delete_vcexpr(left_le);
}

void SolveConstraints::avoid_overflow_sub(VCExpr left, VCExpr right) {
	// -oo <= left + (-right) <= oo
	VCExpr minus_right = vc_bvUMinusExpr(vc, right);
	avoid_overflow_add(left, minus_right);
	delete_vcexpr(minus_right);
}

void SolveConstraints::avoid_overflow_add(VCExpr left, VCExpr right) {
	
	// -oo <= left + right <= oo
	int bit_width = vc_getBVLength(vc, left);
	assert(vc_getBVLength(vc, right) == bit_width);

	VCExpr sum = vc_bv32PlusExpr(vc, left, right);
	VCExpr h_left = vc_bvBoolExtract_One(vc, left, bit_width - 1);
	VCExpr h_right = vc_bvBoolExtract_One(vc, right, bit_width - 1);
	VCExpr h_sum = vc_bvBoolExtract_One(vc, sum, bit_width - 1);
	VCExpr xor_expr = vc_xorExpr(vc, h_left, h_right);
	VCExpr iff_expr = vc_iffExpr(vc, h_right, h_sum);
	VCExpr or_expr = vc_orExpr(vc, xor_expr, iff_expr);

	vc_assertFormula(vc, or_expr);
	
	delete_vcexpr(sum);
	delete_vcexpr(h_left);
	delete_vcexpr(h_right);
	delete_vcexpr(h_sum);
	delete_vcexpr(xor_expr);
	delete_vcexpr(iff_expr);
	delete_vcexpr(or_expr);
}

void SolveConstraints::avoid_overflow_mul(VCExpr left, VCExpr right) {
#if 0
	// TODO: does not support negative numbers
	/*
	 * FIXME: impliesExpr doesn't work as expected. 
	 * We expected to use it to get around the "div-by-zero" problem.
	 * Therefore, we extend the operands to 64-bit integers, and check
	 * whether the product is really out of range. 
	 */
	// left >= 0, right >= 0, left * right <= oo
	{
		vc_assertFormula(vc, vc_sbvGeExpr(vc, left, vc_zero(vc)));
		vc_assertFormula(vc, vc_sbvGeExpr(vc, right, vc_zero(vc)));
		VCExpr long_product = vc_bvMultExpr(
				vc, 64,
				vc_bvSignExtend(vc, left, 64), vc_bvSignExtend(vc, right, 64));
		vc_assertFormula(
				vc, vc_sbvLeExpr(vc, long_product, vc_int_max_64(vc)));
	}
#endif
#if 0
	// left > 0 => right <= oo / left
	vc_assertFormula(
			vc,
			vc_impliesExpr(
				vc,
				vc_sbvGtExpr(vc, left, vc_zero(vc)),
				vc_sbvLeExpr(
					vc,
					right,
					vc_sbvDivExpr(vc, 32, vc_int_max(vc), left))));
	// right > 0 => left <= oo / right
	vc_assertFormula(
			vc,
			vc_impliesExpr(
				vc,
				vc_sbvGtExpr(vc, right, vc_zero(vc)),
				vc_sbvLeExpr(
					vc,
					left,
					vc_sbvDivExpr(vc, 32, vc_int_max(vc), right))));
#endif
}

void SolveConstraints::avoid_overflow(unsigned op, VCExpr left, VCExpr right) {
	switch (op) {
		case Instruction::Add:
#ifdef CHECK_BOUND
			avoid_overflow_add(left, right);
#endif
			break;
		case Instruction::Sub:
#ifdef CHECK_BOUND
			avoid_overflow_sub(left, right);
#endif
			break;
		case Instruction::Mul:
#ifdef CHECK_BOUND
			avoid_overflow_mul(left, right);
#endif
			break;
		case Instruction::UDiv:
		case Instruction::SDiv:
		case Instruction::URem:
		case Instruction::SRem:
#ifdef CHECK_DIV
			avoid_div_by_zero(left, right);
#endif
			break;
		case Instruction::Shl:
#ifdef CHECK_BOUND
			avoid_overflow_shl(left, right);
#endif
			break;
	}
}

void SolveConstraints::print_assertions() {
	assert(vc);
	vc_printAsserts(vc);
}

void SolveConstraints::delete_vcexpr(VCExpr e) {
	if (getExprKind(e) == SYMBOL)
		return;
	vc_DeleteExpr(e);
}
