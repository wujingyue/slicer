#ifndef __SLICER_CAPTURE_H
#define __SLICER_CAPTURE_H

#include "llvm/Module.h"
using namespace llvm;

#include <vector>
using namespace std;

#include "expression.h"

namespace slicer {

	// Constraint (a, b) means (a <= b).
	typedef pair<Expr, Expr> Constraint;

	struct CaptureConstraints: public ModulePass {

		static char ID;

		const static unsigned INVALID_VAR_ID = (unsigned)-1;

		typedef DenseMap<Value *, pair<Expr, Expr> > ValueBoundsInBB;
		typedef pair<Value *, pair<Expr, Expr> > BoundsInBB;

		CaptureConstraints();
		virtual ~CaptureConstraints();
		
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;
		unsigned get_num_constraints() const;
		const Clause *get_constraint(unsigned i) const;

	private:
		void capture_in_func(Function *f);
		void declare_bounds_in_func(Function *f);
		void simplify_constraints();
		void calc_end_bb_bounds(BasicBlock *bb, ValueBoundsInBB &end_bb_bounds);
		// TODO: <bb> is not necessary. 
		void gen_bounds(Value *v, ValueBoundsInBB &end_bb_bounds);
		void gen_bound_of_user(User *v, ValueBoundsInBB &end_bb_bounds);
		void collect_inter_bb_constraints(
				BasicBlock *x, BasicBlock *y, const ValueBoundsInBB &end_bb_bounds);
		void collect_branch_constraints(
				Value *v1, Value *v2, CmpInst::Predicate pred,
				const ValueBoundsInBB &end_bb_bounds,
				vector<BoundsInBB> &branch_bounds);
		// Collect branch constraints for a particular Value. 
		// <vx> may be a constant. If so, don't look it up in the hash table. 
		void link_edge(
				BasicBlock *x, Value *vx, BasicBlock *y, Value *vy,
				const ValueBoundsInBB &end_bb_bounds, // x
				const vector<BoundsInBB> &branch_bounds, // x
				const ValueBoundsInBB &start_bb_bounds); // y
		Expr get_lower_bound(Value *v, const ValueBoundsInBB &end_bb_bounds);
		Expr get_upper_bound(Value *v, const ValueBoundsInBB &end_bb_bounds);

		void print_bounds_in_bb(
				raw_ostream &O, const ValueBoundsInBB &bounds) const;
		void print_bool_expr(
				raw_ostream &O, const BoolExpr &be) const;
		void print_constraint(
				raw_ostream &O, const Constraint &c) const;
		void print_clause(
				raw_ostream &O, const Clause *c) const;

		static Expr get_const_expr(ConstantInt *v);
		static bool is_int_operation(unsigned opcode);
		static string get_symbol_name(unsigned sym_id);
		Expr create_new_symbol();
		static bool is_infty(const Expr &e);
		static bool is_infty_large(const Expr &e);
		static bool is_infty_small(const Expr &e);
		static Expr get_infty_large();
		static Expr get_infty_small();
		static Expr create_expr(
				const string &op, const Expr &op1, const Expr &op2);

		DenseMap<BasicBlock *, ValueBoundsInBB> start_bb_bounds;
		unsigned n_symbols;
		vector<Clause *> constraints;
	};
}

#endif
