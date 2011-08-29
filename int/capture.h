/**
 * Author: Jingyue
 *
 * Capture constraints in a program. 
 */

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
#include "llvm/Analysis/LoopInfo.h"
#include "common/include/typedefs.h"
#include "common/cfg/icfg.h"
using namespace llvm;

#include <vector>
#include <map>
using namespace std;

#include "expression.h"
// FIXME: forward declaration is enough
#include "max-slicing/region-manager.h"

namespace slicer {
	struct CaptureConstraints: public ModulePass {
		const static unsigned INVALID_VAR_ID = (unsigned)-1;

		static char ID;
		CaptureConstraints();
		virtual ~CaptureConstraints();
		virtual bool runOnModule(Module &M);
		void recalculate(Module &M);
		void calculate(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

		unsigned get_num_constraints() const;
		const Clause *get_constraint(unsigned i) const;
		/**
		 * Compute a finger print of all the constraints captured. 
		 * Used to check whether the iterative process should stop. 
		 */
		long get_fingerprint() const;
		bool is_reachable_integer(const Value *v) const;
		/**
		 * Is <v> an integer that's defined only once?
		 */
		bool is_fixed_integer(const Value *v) const;
		/**
		 * Returns all reachable integers or pointers. 
		 */
		const ConstValueSet &get_fixed_integers() const;
		/**
		 * Used by the solver as well, so need make it public.
		 * Called internally by the module to capture constraints on
		 * unreachable BBs. 
		 */
		Clause *get_avoid_branch(const TerminatorInst *ti, unsigned i) const;
		/**
		 * Used by the solver. 
		 * Get the constraints on the loop index of loop <L>. 
		 * NOTE: <constraints> will be cleared at the beginning. 
		 */
		void get_in_loop(const Loop *l, vector<Clause *> &loop_constraints);
		/**
		 * NOTE: <loop_body_constraints> will be cleared at the beginning. 
		 */
		void get_in_loop_body(const Loop *l,
				vector<Clause *> &loop_body_constraints);
		/**
		 * NOTE: <constraints> will be cleared at the beginning. 
		 * Return true if we can figure out the loop bound. 
		 * If this function returns false, we needn't run <get_in_loop_body>. 
		 */
		bool get_loop_bound(const Loop *l, vector<Clause *> &loop_constraints);
		/**
		 * Attach expressions with the given context. 
		 * Won't change the contexts of values that are defined only once
		 * (e.g. global variables or instructions executed only once). 
		 */
		void attach_context(Clause *c, unsigned context);
		void attach_context(BoolExpr *be, unsigned context);
		void attach_context(Expr *e, unsigned context);

	private:
		// Utility functions. 
		static void print_value(raw_ostream &O, const Value *v);
		static Value *get_pointer_operand(const Instruction *i);
		static Value *get_value_operand(const Instruction *i);
		static bool is_power_of_two(uint64_t a, uint64_t &e);
		static bool print_progress(raw_ostream &O, unsigned cur, unsigned tot);
		/**
		 * Wrappers to AdvancedAlias and BddAliasAnalysis. 
		 * Most of the time, we need either may-alias or must-alias, but not
		 * both of them.
		 */
		bool is_using_advanced_alias();
		bool may_alias(const Value *v1, const Value *v2);
		bool must_alias(const Value *v1, const Value *v2);
		/**
		 * Create a constraint saying lb (<= or <) v && v (<= or <) ub.
		 * Q: Why not just have an inclusive mode, and translate a < b into
		 * a <= b - 1? 
		 * A: b - 1 requires bound checks. 
		 * Think about a <= i1 <= b - 1 and b <= i2 <= c
		 * a = 0, b = 0, c = 1, i1 = 0, i2 = 0 is actually a satisfying assignment. 
		 */
		static Clause *construct_bound_constraint(const Value *v,
				const Value *lb, bool lb_inclusive,
				const Value *ub, bool ub_inclusive);
		// Using visitor mode. 
		void replace_with_loop_bound_version(Clause *c, const Loop *l);
		void replace_with_loop_bound_version(BoolExpr *c, const Loop *l);
		void replace_with_loop_bound_version(Expr *c, const Loop *l);
		
		// Address taken variables. 
		void capture_addr_taken(Module &M);
		/*
		 * store v1, p (including global variable initializers)
		 * v2 = load q
		 * p and q may alias
		 * =>
		 * v2 may = v1
		 */
		void capture_may_assign(Module &M);
		void capture_must_assign(Module &M);
		void capture_overwriting_to(LoadInst *i2);
		Instruction *find_latest_overwriter(Instruction *i2, Value *q);
		BasicBlock *get_idom(BasicBlock *bb);
		Instruction *get_idom(Instruction *ins);
		MicroBasicBlock *get_idom_ip(MicroBasicBlock *mbb);
		Instruction *get_idom_ip(Instruction *ins);
		Instruction *find_nearest_common_dom(Instruction *i1, Instruction *i2);
		// Check if any instruction between <i1> and <i2> may write to <q>. 
		// <i1> must dominate <i2>, and they are in the same function. 
		// We don't consider <i1> and <i2> in the path, i.e. the path is an
		// exclusive region (i1, i2). 
		bool path_may_write(
				const Instruction *i1, const Instruction *i2, const Value *q);
		bool path_may_write(int thr_id, size_t trunk_id,
				const Instruction *i2, const Value *q);
		bool path_may_write(const Instruction *i1,
				int thr_id, size_t trunk_id, const Value *q);
		bool blocks_may_write(
				const DenseSet<const ICFGNode *> &blocks,
				const InstList &starts, const InstList &ends, const Value *q);
		bool region_may_write(const Region &r, const Value *q);
		// Check if instruction <i> may write to <q>. 
		bool may_write(
				const Instruction *i, const Value *q, ConstFuncSet &visited_funcs);
		bool may_write(
				const Function *f, const Value *q, ConstFuncSet &visited_funcs);
		
		// General functions. 
		void simplify_constraints();
		void stat(Module &M);
		void setup(Module &M);
		void check_loop(Loop *l);
		void check_loops(Module &M);

		// Integer and pointer values. 
		void capture_top_level(Module &M);
		void identify_fixed_integers(Module &M);
		/* 
		 * Extract constants from constant <c>.
		 * <c> might be a constant expression, so we need extract constants
		 * recursively.
		 */
		void extract_from_consts(Constant *c);
		// TODO: We care about Instructions and ConstantExprs only. We could use
		// Operator as the argument. 
		Clause *get_in_user(const User *user);
		Clause *get_in_argument(const Argument *arg);
		/*
		 * These get_* functions need to check whether their operands
		 * are integers. 
		 */
		Clause *get_in_icmp(const ICmpInst *icmp);
		Clause *get_in_select(const SelectInst *si);
		Clause *get_in_unary(const User *user);
		Clause *get_in_binary(const User *user, unsigned opcode);
		Clause *get_in_gep(const User *user);
		Clause *get_in_phi(const PHINode *phi);
		/**
		 * Returns true if
		 * 1. x is shallower than y OR
		 * 2. x and y are at the same level, but x => y is a backedge of
		 *    their containing loop. 
		 */
		bool comes_from_shallow(const BasicBlock *x, const BasicBlock *y);
		/* Constraints from unreachable blocks. */
		void capture_unreachable(Module &M);
		/**
		 * <unreachable_constraints> will be reset. 
		 */
		void get_unreachable_in_function(Function &F,
				vector<Clause *> &unreachable_constraints);
		/* Function summaries. */
		void capture_function_summaries(Module &M);
		void capture_libcall(const CallSite &cs);
		bool capture_memory_allocation(
				const CallSite &cs, Expr *&start, Expr *&size);
		void add_constraint(Clause *c);

		// Data members. 
		vector<Clause *> constraints;
		ConstValueSet fixed_integers;
		const Type *int_type;
		DominatorTreeBase<ICFGNode> IDT;
	};
}

#endif
