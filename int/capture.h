#ifndef __SLICER_CAPTURE_H
#define __SLICER_CAPTURE_H

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Instructions.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/DominatorInternals.h"
#include "common/include/typedefs.h"
#include "common/cfg/icfg.h"
using namespace llvm;

#include <vector>
#include <map>
using namespace std;

#include "expression.h"

namespace slicer {

	struct CaptureConstraints: public ModulePass {

		static char ID;

#if 0
		enum EdgeType {
			EDGE_CALL,
			EDGE_INTER_BB,
			EDGE_INTRA_BB,
			EDGE_RET
		};
#endif

		const static unsigned INVALID_VAR_ID = (unsigned)-1;

		typedef DenseMap<Value *, pair<Expr, Expr> > ValueBoundsInBB;
		typedef pair<Value *, pair<Expr, Expr> > BoundsInBB;

		CaptureConstraints();
		virtual ~CaptureConstraints();
		virtual bool runOnModule(Module &M);
		bool recalculate(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

		void replace_aa(AliasAnalysis *new_AA);

		unsigned get_num_constraints() const;
		const Clause *get_constraint(unsigned i) const;
		long get_fingerprint() const;
		/* In <constants>? */
		bool is_constant(const Value *v) const;
		/*
		 * Used by the solver as well, so need make it public.
		 * Called internally by the module to capture constraints on
		 * unreachable BBs. 
		 */
		Clause *get_avoid_branch(const TerminatorInst *ti, unsigned i) const;
		/*
		 * Check whether <bb> is the unreachable BB we added. 
		 * It not only has an UnreachableInst at the end, but also has
		 * an llvm.trap instruction. 
		 */
		static bool is_unreachable(const BasicBlock *bb);

	private:
#if 0
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
#endif
		static void print_value(raw_ostream &O, const Value *v);
		// Address taken variables. 
		void capture_addr_taken(Module &M);
		/*
		 * i1: store v1, p || v1 = load p
		 * i2: v2 = load q
		 * i1 dominates i2
		 * p and q must alias
		 * No other store instructions that may overwrite p/q along the way 
		 * =>
		 * v1 = v2
		 */
		void capture_overwritten_in_func(Function *fi);
		void capture_overwriting_to(LoadInst *i2);
		Instruction *find_latest_overwriter(Instruction *i2, Value *q);
		BasicBlock *get_idom(BasicBlock *bb);
		Instruction *get_idom(Instruction *ins);
		MicroBasicBlock *get_idom_ip(MicroBasicBlock *mbb);
		Instruction *get_idom_ip(Instruction *ins);
		// Check if any instruction between <i1> and <i2> may write to <q>. 
		// <i1> must dominate <i2>, and they are in the same function. 
		bool path_may_write(
				const Instruction *i1, const Instruction *i2, const Value *q);
		// Check if instruction <i> may write to <q>. 
		bool may_write(
				const Instruction *i, const Value *q, ConstFuncSet &visited_funcs);
		bool may_write(
				const Function *f, const Value *q, ConstFuncSet &visited_funcs);
#if 0
		void add_addr_taken_eq(Value *v1, Value *v2);
		void get_all_sources(Instruction *ins, Value *p, ValueList &srcs);
		void search_all_sources(
				MicroBasicBlock *mbb, MicroBasicBlock::iterator ins,
				Value *p, ValueList &srcs);
#endif
		Value *get_pointer_operand(Instruction *i) const;
		Value *get_value_operand(Instruction *i) const;
		const Value *get_pointer_operand(const Instruction *i) const;
		const Value *get_value_operand(const Instruction *i) const;
		// General functions. 
		void simplify_constraints();
		void stat(Module &M);
		void setup(Module &M);

#if 0
		static void print_alias_set(raw_ostream &O, const ConstValueSet &as);
#endif

#if 0
		static Expr get_const_expr(ConstantInt *v);
		static string get_symbol_name(unsigned sym_id);
		Expr create_new_symbol();
		static bool is_infty(const Expr &e);
		static bool is_infty_large(const Expr &e);
		static bool is_infty_small(const Expr &e);
		static Expr get_infty_large();
		static Expr get_infty_small();
		static Expr create_expr(
				const string &op, const Expr &op1, const Expr &op2);
#endif
		static bool is_int_operation(unsigned opcode);
		/* Integer constants and pointer constants */
		void identify_constants(Module &M);
		/* 
		 * Extract constants from constant <c>.
		 * <c> might be a constant expression, so we need extract constants
		 * recursively.
		 */
		void extract_consts(Constant *c);
		void add_eq_constraint(Value *v1, Value *v2);
		void capture_constraints_on_consts(Module &M);
		// TODO: We care about Instructions and ConstantExprs only. We could use
		// Operator as the argument. 
		void capture_in_user(User *user);
		/*
		 * These capture_* functions need to check whether their operands
		 * are constant.
		 */
		void capture_in_icmp(ICmpInst *user);
		void capture_in_unary(User *user);
		void capture_in_binary(User *user, unsigned opcode);
		void capture_in_gep(User *user);
		void capture_in_phi(PHINode *phi);
		/* Constraints from unreachable blocks. */
		void capture_unreachable(Module &M);
		void capture_unreachable_in_func(Function *f);
		/* Function summaries. */
		void capture_func_summaries(Module &M);
		void capture_libcall(const CallSite &cs);
		/* Utility functions. */
		/* Returns the size of a type in bits. */
		static unsigned get_type_size(const Type *type);

#if 0
		DenseMap<BasicBlock *, ValueBoundsInBB> start_bb_bounds;
		unsigned n_symbols;
		vector<ValuePair> addr_taken_eqs;
#endif
		vector<Clause *> constraints;
		ValueSet constants;
		const Type *int_type;
		AliasAnalysis *AA;
		DominatorTreeBase<ICFGNode> IDT;
	};
}

#endif