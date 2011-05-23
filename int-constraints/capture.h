#ifndef __SLICER_CAPTURE_H
#define __SLICER_CAPTURE_H

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Instructions.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "common/include/typedefs.h"
using namespace llvm;

#include <vector>
#include <map>
using namespace std;

#include "expression.h"

namespace slicer {

	// Constraint (a, b) means (a <= b).
	typedef pair<Expr, Expr> Constraint;

	struct CaptureConstraints: public ModulePass {

		static char ID;

		enum EdgeType {
			EDGE_CALL,
			EDGE_INTER_BB,
			EDGE_INTRA_BB,
			EDGE_RET
		};

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
		// TODO: Need a better name. 
		void capture_in_func(Function *f);
		void declare_bounds_in_func(Function *f);
		void calc_end_bb_bounds(BasicBlock *bb, ValueBoundsInBB &end_bb_bounds);
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
		// Address taken variables. 
		void capture_addr_taken(Module &M);
		void add_addr_taken_eq(Value *v1, Value *v2);
		void get_all_sources(Instruction *ins, Value *p, ValueList &srcs);
		void search_all_sources(
				MicroBasicBlock *mbb, MicroBasicBlock::iterator ins,
				Value *p, ValueList &srcs);

		void simplify_constraints();

		void stat(Module &M);

		static void print_bounds_in_bb(
				raw_ostream &O, const ValueBoundsInBB &bounds);
		static void print_bool_expr(
				raw_ostream &O, const BoolExpr &be);
		static void print_constraint(
				raw_ostream &O, const Constraint &c);
		static void print_clause(
				raw_ostream &O, const Clause *c);
		static void print_value(raw_ostream &O, const Value *v);
#if 0
		static void print_alias_set(raw_ostream &O, const ConstValueSet &as);
#endif

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
		vector<ValuePair> addr_taken_eqs;
	};
}

#endif
