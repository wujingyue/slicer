/**
 * Author: Jingyue
 *
 * TODO: simplify realize functions. Extra common conde into a function. 
 */

#define DEBUG_TYPE "int"

#include <list>
#include <iostream>
#include <sstream>
using namespace std;

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Target/TargetData.h"
using namespace llvm;

#include "rcs/util.h"
#include "rcs/IntraReach.h"
#include "rcs/FPCallGraph.h"
#include "rcs/ExecOnce.h"
using namespace rcs;

#include "slicer/capture.h"
#include "slicer/solve.h"
#include "slicer/adv-alias.h"
using namespace slicer;

void SolveConstraints::getAnalysisUsage(AnalysisUsage &AU) const {
	// LLVM 2.9 crashes if I use addRequiredTransitive. 
	AU.setPreservesAll();
	AU.addRequired<TargetData>();
	AU.addRequired<IDAssigner>();
	AU.addRequired<DominatorTree>();
	AU.addRequired<LoopInfo>();
	AU.addRequired<IntraReach>();
	AU.addRequired<CaptureConstraints>();
	AU.addRequired<FPCallGraph>();
	AU.addRequired<ExecOnce>();
}

char SolveConstraints::ID = 0;

SolveConstraints::SolveConstraints(): ModulePass(ID),
	print_counterexample_(false),
	print_asserts_(false), print_minimal_proof_set_(false)
{
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

	// FIXME: Needn't clear <root> actually. Once a == b, a == b forever. 
	// Not the performance bottleneck though. 
	root.clear();
	identify_eqs(); // This step does not require <vc>.
	translate_captured(M);
}

void SolveConstraints::identify_eq(const Value *v1, const Value *v2) {
	const Value *r1 = get_root(v1), *r2 = get_root(v2);

	// Validity check. 
	const Expr *e_r1 = new Expr(r1), *e_r2 = new Expr(r2);
	// Otherwise, the clause should contain Trunc, ZExt, or SExt. 
	assert(e_r1->get_width() == e_r2->get_width());
	delete e_r1;
	delete e_r2;

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

void SolveConstraints::identify_eqs() {
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	for (unsigned i = 0; i < CC.get_num_constraints(); ++i) {
		const Clause *c = CC.get_constraint(i);
		const Value *v1 = NULL, *v2 = NULL;
		if (is_simple_eq(c, &v1, &v2)) {
			assert(v1 && v2);
			identify_eq(v1, v2);
		}
	}
#if 0
	// Dangerous with existence of contexts. 
	assert_not_supported();
	if (AdvancedAlias *AA = getAnalysisIfAvailable<AdvancedAlias>()) {
		vector<ConstValuePair> must_alias_pairs;
		AA->get_must_alias_pairs(must_alias_pairs);
		for (size_t i = 0; i < must_alias_pairs.size(); ++i)
			identify_eq(must_alias_pairs[i].first, must_alias_pairs[i].second);
	}
#endif
}

void SolveConstraints::refine_candidates(list<const Value *> &candidates) {
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();

	list<const Value *>::iterator i, j, to_del;

	for (i = candidates.begin(); i != candidates.end(); ) {
		const Value *v = *i;
		bool to_be_removed = false;
		// Only try fixing the roots of equivalent classes. 
		to_be_removed = to_be_removed || (get_root(v) != v);
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

		VCExpr vce = translate_to_vc(c);
		int ret = try_to_simplify(vce);
		vc_DeleteExpr(vce);
		assert(ret != 0);
		// If can be proved by simplification, don't add it to the constraint set. 
		if (ret == -1)
			update_appeared(appeared, c);

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
	const ValueSet &fixed_integers = CC.get_fixed_integers();
	candidates.insert(candidates.end(),
			fixed_integers.begin(), fixed_integers.end());
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
	vc_DeleteExpr(f);
	dbgs() << "Done\n";
	
	forall(list<const Value *>, it, candidates) {
		const Value *v = *it;
		// Only integers under Context 0 may be fixed. 
		VCExpr vce = translate_to_vc(v, 0);
		VCExpr ce = vc_getCounterExample(vc, vce);
		fixed_values.push_back(make_pair(
					v, make_pair(getBVUnsigned(ce), vc_getBVLength(vc, ce))));
		vc_DeleteExpr(vce);
		vc_DeleteExpr(ce);
	}
	vc_pop(vc);
	
	// Try proving each guess. 
	unsigned n_fixed = 0, n_not_fixed = 0, n_opted = 0;
	for (i = fixed_values.begin(); i != fixed_values.end(); ) {
		vc_push(vc);
		VCExpr guessed_value = vc_bvConstExprFromInt(vc,
				i->second.second, i->second.first);
		VCExpr vce = translate_to_vc(i->first, 0);
		VCExpr eq = vc_eqExpr(vc, vce, guessed_value);
		vc_DeleteExpr(guessed_value);
		vc_DeleteExpr(vce);
		int fixed = vc_query(vc, eq);
		vc_DeleteExpr(eq);

		assert(fixed != 2);
		if (fixed == 1) {
			// <i>'s value must be fixed. Skip to the next candidate. 
			// FIXME: dbgs() does not support colors? 
			errs().changeColor(raw_ostream::GREEN) << "Y"; errs().resetColor();
			++n_fixed;
			++i;
		} else {
			errs().changeColor(raw_ostream::RED) << "N"; errs().resetColor();
			++n_not_fixed;
			j = i; ++j;
			while (j != fixed_values.end()) {
				VCExpr vj = translate_to_vc(j->first, 0);
				VCExpr ce = vc_getCounterExample(vc, vj);
				vc_DeleteExpr(vj);
				if (j->second.first == getBVUnsigned(ce)) {
					++j;
				} else {
					to_del = j;
					++j;
					errs().changeColor(raw_ostream::BLUE) << "O"; errs().resetColor();
					++n_opted;
					fixed_values.erase(to_del);
				}
				vc_DeleteExpr(ce);
			}
			// <i> does not have a fixed value. 
			to_del = i;
			++i;
			fixed_values.erase(to_del);
		} // if (fixed == 1)

		vc_pop(vc);
	}
	
	dbgs() << "\n";
	dbgs() << "fixed = " << n_fixed << "; not fixed = " << n_not_fixed <<
		"; opted = " << n_opted << "\n";
	// Finally, make identified fixed values to be roots. 
	for (i = fixed_values.begin(); i != fixed_values.end(); ++i) {
		const Value *v = i->first;
		assert(isa<IntegerType>(v->getType()));
		// Note: Convert it to a signed integer.
		if (isa<IntegerType>(v->getType())) {
			root[v] = ConstantInt::getSigned(cast<IntegerType>(v->getType()),
					(int)i->second.first);
		} else {
			TargetData &TD = getAnalysis<TargetData>();
			assert(isa<PointerType>(v->getType()));
			root[v] = ConstantInt::getSigned(
					IntegerType::get(v->getContext(), TD.getTypeSizeInBits(v->getType())),
					i->second.first);
		}
	}
}

void SolveConstraints::update_appeared(
		ConstValueSet &appeared, const Clause *c) {
	if (c->be)
		update_appeared(appeared, c->be);
	else if (c->op == Instruction::UserOp1)
		update_appeared(appeared, c->c1);
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

void SolveConstraints::print_minimal_proof_set(const Clause *to_prove) {
	errs() << "Finding a minimal proof set...\n";
	errs() << "Not implemented yet. Use slicer/int/calc-min-proof-set.\n";
}

void SolveConstraints::diagnose(Module &M) {
	errs() << "Detected inconsistency. Diagnosing...\n";

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
			if (try_to_simplify(vce) == -1)
				vc_assertFormula(vc, vce);
			vc_DeleteExpr(vce);
		}
		VCExpr f = vc_falseExpr(vc);
		int ret = vc_query(vc, f);
		vc_DeleteExpr(f);
		if (ret == 0) {
			errs().changeColor(raw_ostream::GREEN) << "Y"; errs().resetColor();
			does_not_matter.erase(j);
		} else {
			errs().changeColor(raw_ostream::RED) << "N"; errs().resetColor();
		}
	}
	errs() << "\n";

	errs() << "Start printing a minimal set...\n";
	for (unsigned i = 0; i < n_constraints; ++i) {
		if (!does_not_matter.count(i)) {
			print_clause(dbgs(), CC.get_constraint(i), getAnalysis<IDAssigner>());
			errs() << "\n";
		}
	}
	errs() << "Finished printing a minimal set\n";
}

#if 0
void SolveConstraints::separate(Module &M) {
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	unsigned n_constraints = CC.get_num_constraints();

	ConstValueMapping root2;
	for (unsigned i = 0; i < n_constraints; ++i) {
		Clause *c = CC.get_constraint(i)->clone();
		replace_with_root(c);
		VCExpr vce = translate_to_vc(c);
		if (try_to_simplify(vce) == -1) {
			ConstValueSet appeared;
			update_appeared(appeared, c);
			const Value *last_v = NULL;
			forall(ConstValueSet, it, appeared) {
				const Value *v = *it;
				if (!isa<Constant>(v)) {
					if (last_v) {
						const Value *r_last_v = get_root2(root2, last_v);
						const Value *r_v = get_root2(root2, v);
						root2[r_v] = r_last_v;
					}
					last_v = v;
				}
			}
		}
		vc_DeleteExpr(vce);
		delete c;
	}

	DenseMap<const Value *, unsigned> class_sizes;
	forallconst(ConstValueSet, it, CC.get_integers()) {
		const Value *v = *it;
		const Value *r_v = get_root2(root2, v);
		++class_sizes[r_v];
	}

	vector<unsigned> sorted_class_sizes;
	for (DenseMap<const Value *, unsigned>::iterator it = class_sizes.begin();
			it != class_sizes.end(); ++it) {
		if (it->second > 1)
			sorted_class_sizes.push_back(it->second);
	}
	sort(sorted_class_sizes.begin(), sorted_class_sizes.end(),
			greater<unsigned>());
	errs() << "Sizes of related classes:\n";
	for (size_t i = 0; i < sorted_class_sizes.size(); ++i) {
		errs() << sorted_class_sizes[i] << "\n";
	}
}
#endif

void SolveConstraints::translate_captured(Module &M) {
#if 0
	separate(M);
#endif

	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	unsigned n_constraints = CC.get_num_constraints();
	dbgs() << "# of captured constraints = " << n_constraints << "\n";

	assert(vc);
	for (unsigned i = 0; i < n_constraints; ++i) {
		Clause *c = CC.get_constraint(i)->clone();

		VCExpr vce = translate_to_vc(c);
		if (try_to_simplify(vce) == -1)
			vc_assertFormula(vc, vce);
		vc_DeleteExpr(vce);
		
		delete c; // c is cloned
	}

	// The captured constraints should be consistent. 
	check_consistency(M);
}

void SolveConstraints::check_consistency(Module &M) {
	dbgs() << "Checking consistency... ";
	vc_push(vc);
	if (print_asserts_) {
		vc_printVarDecls(vc);
		vc_printAsserts(vc);
		cout << "QUERY FALSE;\n";
	}
	VCExpr f = vc_falseExpr(vc);
	int ret = vc_query(vc, f);
	vc_DeleteExpr(f);
	vc_pop(vc);

	if (ret != 0) {
		diagnose(M);
		// CC.print(errs(), &M);
	}
	assert(ret == 0 && "The captured constraints is inconsistent.");
	dbgs() << "Done\n";
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
	else if (c->op == Instruction::UserOp1)
		replace_with_root(c->c1);
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

const Value *SolveConstraints::get_root2(
		ConstValueMapping &root2, const Value *x) {
	assert(x);
	if (!root2.count(x))
		return x;
	const Value *y = root2.lookup(x);
	if (y != x) {
		const Value *ry = get_root2(root2, y);
		root2[x] = ry;
	}
	return root2.lookup(x);
}

bool SolveConstraints::is_simple_eq(
		const Clause *c, const Value **v1, const Value **v2) {
	if (c->be == NULL)
		return false;
	if (c->be->p != CmpInst::ICMP_EQ)
		return false;
	// <is_simple_eq> ignores Expr::LoopBound. 
	const Expr *e1 = c->be->e1, *e2 = c->be->e2;
	if (e1->type != Expr::SingleDef || e2->type != Expr::SingleDef)
		return false;
	// Must be fixed integers. 
	if (e1->context != 0 || e2->context != 0)
		return false;
	if (v1)
		*v1 = e1->v;
	if (v2)
		*v2 = e2->v;
	return true;
}

void SolveConstraints::print(raw_ostream &O, const Module *M) const {
	// Don't know what to do. 
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
	delete c2;

	Clause *not_c = new Clause(Instruction::UserOp1, c->clone());
	bool sat = !provable(not_c);
	delete not_c;

	return sat;
}

bool SolveConstraints::provable(const Clause *c) {
	DEBUG(dbgs() << "Provable?: ";
			print_clause(dbgs(), c, getAnalysis<IDAssigner>());
			dbgs() << "\n";);

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

	if (print_asserts_) {
		vc_printVarDecls(vc);
		vc_printAsserts(vc);
		cout << "QUERY ";
		vc_printExpr(vc, vce);
		cout << ";\n";
	}

	int ret = vc_query(vc, vce);
	assert(ret != 2);
	vc_DeleteExpr(vce);
	if (ret == 0 && print_counterexample_)
		print_counterexample();
	vc_pop(vc);

	if (ret == 1 && print_minimal_proof_set_)
		print_minimal_proof_set(c);

	return ret == 1;
}

void SolveConstraints::realize(const Clause *c) {
	if (c->be)
		realize(c->be);
	else if (c->op == Instruction::UserOp1)
		realize(c->c1);
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
		realize(e->u, e->context);
	} else if (e->type == Expr::SingleDef) {
		realize(e->callstack, e->v, e->context);
	} else {
		assert(e->type == Expr::LoopBound);
		assert(false && "Type LoopBound shouldn't be used in a query");
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

void SolveConstraints::realize(const Use *u, unsigned context) {
	// The value of a llvm::Constant is compile-time known. Therefore,
	// we don't need to capture extra constraints. 
	if (const Instruction *ins = dyn_cast<Instruction>(u->getUser()))
		realize(ins, context);
}

void SolveConstraints::realize(const ConstInstList &callstack,
		const Value *v, unsigned context) {
	// Realize <ins> itself. 
	if (const Instruction *ins = dyn_cast<Instruction>(v))
		realize(ins, context);
	// Realize each call instruction. 
	for (size_t i = 0; i < callstack.size(); ++i)
		realize(callstack[i], context);
	// Realize each call edge in the calling context. 
	for (size_t i = 0; i + 1 < callstack.size(); ++i) {
		realize_function_call(callstack[i],
				callstack[i + 1]->getParent()->getParent(), context);
	}
	if (callstack.size() > 0) {
		if (const Function *container = get_container(v))
			realize_function_call(callstack.back(), container, context);
	}
}

const Function *SolveConstraints::get_container(const Value *v) {
	if (const Instruction *ins = dyn_cast<Instruction>(v))
		return ins->getParent()->getParent();
	if (const Argument *arg = dyn_cast<Argument>(v))
		return arg->getParent();
	// TODO: Handle ConstantExpr. 
	return NULL;
}

void SolveConstraints::realize_dominating_loops(const BasicBlock *bb,
		unsigned context) {
	// Run DominatorTree before running LoopInfo; otherwise, LoopInfo
	// would be invalidated. 
	DominatorTree &DT = getAnalysis<DominatorTree>(
			*const_cast<Function *>(bb->getParent()));

	vector<Loop *> loops;
	get_all_loops(bb->getParent(), loops);
	
	vector<Loop *> dominating_loops;
	for (size_t i = 0; i < loops.size(); ++i) {
		if (DT.dominates(loops[i]->getHeader(), bb))
			dominating_loops.push_back(loops[i]);
	}

	for (size_t i = 0; i < dominating_loops.size(); ++i)
		realize_dominating_loop(dominating_loops[i], context);
}

void SolveConstraints::get_all_loops(const Function *f,
		vector<Loop *> &loops) {
	LoopInfo &LI = getAnalysis<LoopInfo>(*const_cast<Function *>(f));
	for (LoopInfo::iterator lii = LI.begin(); lii != LI.end(); ++lii)
		get_all_loops(*lii, loops);
}

void SolveConstraints::get_all_loops(Loop *l, vector<Loop *> &loops) {
	loops.push_back(l);
	for (Loop::iterator li = l->begin(); li != l->end(); ++li)
		get_all_loops(*li, loops);
}

void SolveConstraints::realize_dominating_loop(const Loop *l,
		unsigned context) {
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();

	vector<Clause *> constraints_from_l;
	CC.get_in_loop(l, constraints_from_l);
	for (size_t i = 0; i < constraints_from_l.size(); ++i) {
		Clause *c = constraints_from_l[i];
		Clause *c2 = c->clone();
		CC.attach_context(c2, context);
		replace_with_root(c2); // Only fixed integers will be replaced. 

		VCExpr vce = translate_to_vc(c2);
		int simplified = try_to_simplify(vce);
		assert(simplified != 0);
		if (simplified == -1) {
			DEBUG(dbgs() << "[realize] ";
					print_clause(dbgs(), c2, getAnalysis<IDAssigner>());
					dbgs() << "\n";);
			vc_assertFormula(vc, vce);
		}
		vc_DeleteExpr(vce);

		delete c2;
	}
	for (size_t i = 0; i < constraints_from_l.size(); ++i)
		delete constraints_from_l[i];
}

void SolveConstraints::realize_containing_loops(const BasicBlock *bb,
		unsigned context) {
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	LoopInfo &LI = getAnalysis<LoopInfo>(*const_cast<Function *>(bb->getParent()));

	Loop *l = LI.getLoopFor(bb);
	while (l) {
		vector<Clause *> constraints_from_l;
		CC.get_in_loop(l, constraints_from_l);
		for (size_t i = 0; i < constraints_from_l.size(); ++i) {
			Clause *c = constraints_from_l[i];
			Clause *c2 = c->clone();
			CC.attach_context(c2, context);
			replace_with_root(c2); // Only fixed integers will be replaced. 

			VCExpr vce = translate_to_vc(c2);
			int simplified = try_to_simplify(vce);
			assert(simplified != 0);
			if (simplified == -1) {
				DEBUG(dbgs() << "[realize] ";
						print_clause(dbgs(), c2, getAnalysis<IDAssigner>());
						dbgs() << "\n";);
				vc_assertFormula(vc, vce);
			}
			vc_DeleteExpr(vce);

			delete c2;
		}
		for (size_t i = 0; i < constraints_from_l.size(); ++i)
			delete constraints_from_l[i];

		l = l->getParentLoop();
	}
}

void SolveConstraints::realize_function_call(const Instruction *ins,
		const Function *f, unsigned context) {
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();

	CallSite cs(const_cast<Instruction *>(ins));
	assert(cs.getInstruction() && "<ins> is not a CallInst/InvokeInst");

	if (cs.arg_size() != f->getArgumentList().size()) {
		errs() << "[Warning] Signatures don't match: " << f->getName() << "\n";
		errs() << *ins << "\n";
		return;
	}

	unsigned i = 0;
	for (Function::const_arg_iterator ai = f->arg_begin();
			ai != f->arg_end(); ++ai) {
		const Value *actual = cs.getArgument(i);
		const Value *formal = ai;
		
		Clause *c = new Clause(new BoolExpr(CmpInst::ICMP_EQ,
					new Expr(actual), new Expr(formal)));
		Clause *c2 = c->clone();
		CC.attach_context(c2, context);
		replace_with_root(c2);
		VCExpr vce = translate_to_vc(c2);
		if (try_to_simplify(vce) != 1) {
			DEBUG(dbgs() << "[realize] ";
					print_clause(dbgs(), c2, getAnalysis<IDAssigner>());
					dbgs() << "\n";);
			vc_assertFormula(vc, vce);
		}
		vc_DeleteExpr(vce);
		delete c2;
		delete c;

		++i;
	}
}

void SolveConstraints::realize(const Function *f, unsigned context) {
	ExecOnce &EO = getAnalysis<ExecOnce>();
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	// If <f> can be executed only once, its constraints are already
	// captured by the capturer. 
	if (EO.executed_once(f))
		return;

	vector<Clause *> constraints_in_f;
	CC.get_in_function(f, constraints_in_f);
	for (vector<Clause *>::iterator itr = constraints_in_f.begin();
			itr != constraints_in_f.end(); ++itr) {
		Clause *c2 = (*itr)->clone();
		CC.attach_context(c2, context);
		replace_with_root(c2);
		VCExpr vce = translate_to_vc(c2);
		if (try_to_simplify(vce) != 1) {
			DEBUG(dbgs() << "[realize] ";
					print_clause(dbgs(), c2, getAnalysis<IDAssigner>());
					dbgs() << "\n";);
			vc_assertFormula(vc, vce);
		}
		vc_DeleteExpr(vce);
		delete c2;
		delete *itr;
	}
}

void SolveConstraints::realize(const Instruction *ins, unsigned context) {
	/**
	 * Realize its containing functions and containing loops. 
	 * Fix branches along the way. 
	 */
	assert(ins);
	BasicBlock *bb = const_cast<BasicBlock *>(ins->getParent());
	Function *f = bb->getParent();

	// Realize its containing function. <realize> does nothing if the function
	// is executed only once. 
	realize(f, context);

	// Realize the caller of its containing functions. Calling contexts would
	// help a lot with resolving the ambiguity. 
	FPCallGraph &CG = getAnalysis<FPCallGraph>();
	InstList call_sites = CG.getCallSites(f);
	if (call_sites.size() == 1)
		realize(call_sites[0], context);

	// Realize each dominating loop.
	realize_dominating_loops(bb, context);
	// realize_containing_loops(bb, context);

	// Realize each dominating BranchInst. 
	realize_dominating_branches(bb, context);

	// PHI edges from inside of loops to outside of loops. 
	realize_dominating_loop_exits(bb, context);
}

void SolveConstraints::realize_dominating_loop_exits(BasicBlock *bb,
		unsigned context) {
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();

	while (bb != bb->getParent()->begin()) {
		for (BasicBlock::iterator ins = bb->begin();
				bb->getFirstNonPHI() != ins; ++ins) {
			PHINode *phi = dyn_cast<PHINode>(ins); assert(phi);
			// TODO: Can we do the similar thing for multiple incoming edges? 
			// So far it's necessary because we want the incoming block to
			// dominate the current BB. 
			if (phi->getNumIncomingValues() == 1) {
				if (!CC.comes_from_shallow(phi->getIncomingBlock(0), bb)) {
					Clause *c = new Clause(new BoolExpr(CmpInst::ICMP_EQ,
								new Expr(phi), new Expr(phi->getIncomingValue(0))));
					CC.attach_context(c, context);
					replace_with_root(c);

					VCExpr vce = translate_to_vc(c);
					if (try_to_simplify(vce) != 1) {
						DEBUG(dbgs() << "[realize] ";
								print_clause(dbgs(), c, getAnalysis<IDAssigner>());
								dbgs() << "\n";);
						vc_assertFormula(vc, vce);
					}
					vc_DeleteExpr(vce);

					delete c;
				}
			}
		}
		bb = get_idom(bb); assert(bb);
	}
}

void SolveConstraints::realize_dominating_branches(BasicBlock *bb,
		unsigned context) {
	Function *f = bb->getParent();

	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	// TODO: Make it inter-procedural
	IntraReach &IR = getAnalysis<IntraReach>(*f);

	BasicBlock *dom = bb;
	while (dom != &f->getEntryBlock()) {
		BasicBlock *p = get_idom(dom); assert(p);
		/*
		 * If a successor of <p> cannot reach <dom>, the condition that leads
		 * to that successor must not hold. 
		 */ 
		ConstBBSet visited, sink;
		sink.insert(p);
		IR.floodfill_r(dom, sink, visited);

		TerminatorInst *ti = p->getTerminator(); assert(ti);
		for (unsigned i = 0; i < ti->getNumSuccessors(); ++i) {
			if (!visited.count(ti->getSuccessor(i))) {
				if (const Clause *c = CC.get_avoid_branch(ti, i)) {
					Clause *c2 = c->clone();
					CC.attach_context(c2, context);
					replace_with_root(c2);

					VCExpr vce = translate_to_vc(c2);
					if (try_to_simplify(vce) != 1) {
						DEBUG(dbgs() << "[realize] ";
								print_clause(dbgs(), c2, getAnalysis<IDAssigner>());
								dbgs() << "\n";);
						vc_assertFormula(vc, vce);
					}
					vc_DeleteExpr(vce);

					delete c2;
					delete c;
				}
			}
		}
		dom = p;
	}
}

void SolveConstraints::print_counterexample() {
	vc_printCounterExample(vc);
#if 0
	IDAssigner &IDA = getAnalysis<IDAssigner>();
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();

	// Scan through all fixed integers. 
	// TODO: Scan non-fixed integers as well. 
	const ConstValueSet &fixed_integers = CC.get_fixed_integers();
	DenseMap<const Value *, unsigned> assignment;
	forallconst(ConstValueSet, it, fixed_integers) {
		const Value *v = *it;
		const Value *root_v = get_root(v);
		VCExpr e = translate_to_vc(root_v, 0);
		VCExpr ce = vc_getCounterExample(vc, e);
		assignment[v] = getBVUnsigned(ce);
	}

	vector<pair<unsigned, unsigned> > sorted_assignment;
	for (DenseMap<const Value *, unsigned>::iterator it = assignment.begin();
			it != assignment.end(); ++it) {
		const Value *v = it->first;
		unsigned value_id = IDA.getValueID(v);
		sorted_assignment.push_back(make_pair(value_id, it->second));
		if (isa<IntegerType>(v->getType()) && (int)it->second < 0)
			errs() << "[Warning] x" << value_id << " has a strange value.\n";
	}
	sort(sorted_assignment.begin(), sorted_assignment.end());

	errs() << "Start printing a counter example...\n";
	for (size_t i = 0, E = sorted_assignment.size(); i < E; ++i) {
		char hex_str[1024];
		sprintf(hex_str, "%08X", sorted_assignment[i].second);
		errs() << "x" << sorted_assignment[i].first << " = " << hex_str << "\n";
	}
	errs() << "Finished printing the counter example\n";
#endif
}

template <typename T1, typename T2>
bool SolveConstraints::satisfiable(CmpInst::Predicate p,
		const ConstInstList &c1, const T1 *v1,
		const ConstInstList &c2, const T2 *v2) {
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	
	Expr *e1 = new Expr(v1), *e2 = new Expr(v2);
	CC.attach_context(e1, 1);
	CC.attach_context(e2, 2);
	e1->callstack = c1;
	e2->callstack = c2;
	
	const Clause *c = new Clause(new BoolExpr(p, e1, e2));
	bool ret = satisfiable(c);
	delete c;
	return ret;
}

template bool SolveConstraints::satisfiable(CmpInst::Predicate,
		const ConstInstList &, const Value *, const ConstInstList &, const Value *);
template bool SolveConstraints::satisfiable(CmpInst::Predicate,
		const ConstInstList &, const Value *, const ConstInstList &, const Use *);
template bool SolveConstraints::satisfiable(CmpInst::Predicate,
		const ConstInstList &, const Use *, const ConstInstList &, const Value *);
template bool SolveConstraints::satisfiable(CmpInst::Predicate,
		const ConstInstList &, const Use *, const ConstInstList &, const Use *);

template <typename T1, typename T2>
bool SolveConstraints::provable(CmpInst::Predicate p,
		const ConstInstList &c1, const T1 *v1,
		const ConstInstList &c2, const T2 *v2) {
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();

	Expr *e1 = new Expr(v1), *e2 = new Expr(v2);
	CC.attach_context(e1, 1);
	CC.attach_context(e2, 2);
	e1->callstack = c1;
	e2->callstack = c2;
	
	const Clause *c = new Clause(new BoolExpr(p, e1, e2));
	bool ret = provable(c);
	delete c;
	return ret;
}

template bool SolveConstraints::provable(CmpInst::Predicate,
		const ConstInstList &, const Value *, const ConstInstList &, const Value *);
template bool SolveConstraints::provable(CmpInst::Predicate,
		const ConstInstList &, const Value *, const ConstInstList &, const Use *);
template bool SolveConstraints::provable(CmpInst::Predicate,
		const ConstInstList &, const Use *, const ConstInstList &, const Value *);
template bool SolveConstraints::provable(CmpInst::Predicate,
		const ConstInstList &, const Use *, const ConstInstList &, const Use *);
