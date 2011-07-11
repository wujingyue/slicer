/**
 * Author: Jingyue
 */

// TODO: Supports 32-bit unsigned integers only. 

#ifndef __SLICER_SOLVE_H
#define __SLICER_SOLVE_H

#include "llvm/System/Mutex.h"
using namespace llvm;

#include "expression.h"

#define Expr VCExpr
#define Type VCType
#include "stp/c_interface.h"
#undef Expr
#undef Type

namespace slicer {

	// SolveConstraints runs CaptureConstraints to capture
	// existing constraints firstly. Then, the user may add
	// new constraints, and ask SolveConstraints whether there's
	// an assignment satisfying both existing constraints and
	// newly-added constraints. 
	struct SolveConstraints: public ModulePass {

		static char ID;

		SolveConstraints(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void print(raw_ostream &O, const Module *M) const;
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void releaseMemory();

		bool recalculate(Module &M);
		ConstantInt *get_fixed_value(const Value *v);

		bool satisfiable(const vector<const Clause *> &more_clauses);
		bool satisfiable(const Clause *c) {
			return satisfiable(vector<const Clause *>(1, c));
		}
		template <typename T1, typename T2>
		bool satisfiable(CmpInst::Predicate p, const T1 *v1, const T2 *v2) {
			const Clause *c = new Clause(new BoolExpr(
						p, new Expr(v1), new Expr(v2)));
			bool ret = satisfiable(vector<const Clause *>(1, c));
			delete c;
			return ret;
		}

		bool provable(const vector<const Clause *> &more_clauses);
		bool provable(const Clause *c) {
			return provable(vector<const Clause *>(1, c));
		}
		template <typename T1, typename T2>
		bool provable(CmpInst::Predicate p, const T1 *v1, const T2 *v2) {
			const Clause *c = new Clause(new BoolExpr(
						p, new Expr(v1), new Expr(v2)));
			bool ret = provable(vector<const Clause *>(1, c));
			delete c;
			return ret;
		}

	private:
		/*
		 * Translates and simplifies captured constraints, and
		 * inserts them to <vc>.
		 */
		void translate_captured();
		VCExpr translate_to_vc(const Clause *c);
		VCExpr translate_to_vc(const BoolExpr *be);
		VCExpr translate_to_vc(const Expr *e);
		VCExpr translate_to_vc(const Value *v);
		VCExpr translate_to_vc(const Use *u);
		void avoid_overflow(unsigned op, VCExpr left, VCExpr right);
		// Checks whether <c> is in the form of (v1 == v2).
		// If so, outputs <v1> and <v2> as well. 
		static bool is_simple_eq(
				const Clause *c, const Value **v1, const Value **v2);
		// Updates <root> to reflect simple eqs. 
		void identify_eqs();
		// Updates <root>. Make the identified fixed values the roots. 
		void identify_fixed_values();
		const Value *get_root(const Value *x);
		void replace_with_root(Clause *c);
		void replace_with_root(BoolExpr *be);
		/**
		 * Note that this function changes llvm::Use to llvm::Value, because
		 * it may not be valid to use the original value. 
		 * Therefore, if you want to call realize, call it beforehand. 
		 */
		void replace_with_root(Expr *e);
#if 0
		/**
		 * Returns whether the clause only contains ConstantInt's. 
		 * If so, we can simply discard this clause, because we assume
		 * the captured constraints are consistent. 
		 */
		bool contains_only_constints(const Clause *c) const;
		bool contains_only_constints(const BoolExpr *c) const;
		bool contains_only_constints(const Expr *c) const;
#endif

		static void vc_error_handler(const char *err_msg);
		static VCExpr vc_zero(VC vc) {
			return vc_bv32ConstExprFromInt(vc, 0);
		}
		static VCExpr vc_uint_max(VC vc) {
			return vc_bv32ConstExprFromInt(vc, UINT_MAX);
		}
		static VCExpr vc_int_max(VC vc) {
			return vc_bv32ConstExprFromInt(vc, INT_MAX);
		}
		static VCExpr vc_int_min(VC vc) {
			return vc_bv32ConstExprFromInt(vc, INT_MIN);
		}
		// Returns the same value as <vc_int_max/min>, but in 64-bit. 
		static VCExpr vc_int_max_64(VC vc) {
			return vc_bvConstExprFromInt(vc, 64, INT_MAX);
		}
		static VCExpr vc_int_min_64(VC vc) {
			// NOTE: The statement below is WRONG. 
			// return vc_bvConstExprFromInt(vc, 64, INT_MIN);
			// INT_MIN = 0x80000000. If directly converted into 64-bit, it
			// will be a positive integer. 
			return vc_bvSignExtend(vc, vc_int_min(vc), 64);
		}

		/**
		 * The place the value is used may give us extra constraints. 
		 * This function is designed to capture those constraints. 
		 * It looks at the intra-procedural control flow,
		 * and adds branch conditions that need to be satisfied along the way. 
		 * TODO: inter-procedural
		 */
		void realize(const Clause *c);
		void realize(const BoolExpr *c);
		void realize(const Expr *c);
		void realize(const Use *u);
		void realize(const Instruction *i);
		BasicBlock *get_idom(BasicBlock *bb);
		// Protected by <vc_mutex>.
		static void create_vc();
		static void destroy_vc();

		/* NOTE: <root> may contain some constants that don't appeared in CC. */
		ConstValueMapping root;
		/* There can only be one instance of VC running. */
		static VC vc;
		static sys::Mutex vc_mutex;
	};
}

#endif
