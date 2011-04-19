#ifndef __SLICER_CAPTURE_H
#define __SLICER_CAPTURE_H

#include "llvm/Module.h"
using namespace llvm;

#include <vector>
using namespace std;

namespace slicer {

	struct CaptureConstraints: public ModulePass {

		static char ID;

		const static unsigned INVALID_VAR_ID = (unsigned)-1;

		typedef string Expr;
		typedef DenseMap<Value *, pair<Expr, Expr> > ValueBoundsInBB;

		CaptureConstraints();
		
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;
		unsigned get_num_constraints() const;

	private:
		void print_bounds_in_bb(
				raw_ostream &O, const ValueBoundsInBB &bounds) const;
		void capture_in_func(Function *f);
		void declare_bounds_in_func(Function *f);
		void calc_end_bb_bounds(BasicBlock *bb, ValueBoundsInBB &end_bb_bounds);
		void gen_bounds(
				Value *v, BasicBlock *bb, ValueBoundsInBB &end_bb_bounds);
		void compute_bound(
				User *v, BasicBlock *bb, ValueBoundsInBB &end_bb_bounds);
		Expr get_lower_bound(Value *v, const ValueBoundsInBB &end_bb_bounds);
		Expr get_upper_bound(Value *v, const ValueBoundsInBB &end_bb_bounds);
		Expr get_const_expr(ConstantInt *v);

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
	};
}

#endif
