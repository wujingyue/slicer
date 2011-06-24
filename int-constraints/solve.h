/**
 * Author: Jingyue
 */

// TODO: Supports 32-bit unsigned integers only. 

#ifndef __SLICER_SOLVE_H
#define __SLICER_SOLVE_H

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

		SolveConstraints();
		~SolveConstraints();
		virtual bool runOnModule(Module &M);
		virtual void print(raw_ostream &O, const Module *M) const;
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;

		bool satisfiable(const vector<const Clause *> &more_clauses);
		bool provable(const vector<const Clause *> &more_clauses);

		template <typename T1, typename T2>
		bool may_equal(const T1 *v1, const T2 *v2) {
			const Clause *c = new Clause(new BoolExpr(
						CmpInst::ICMP_EQ,
						new Expr(v1),
						new Expr(v2)));
			bool ret = satisfiable(vector<const Clause *>(1, c));
			delete c;
			return ret;
		}

		template <typename T1, typename T2>
		bool must_equal(const T1 *v1, const T2 *v2) {
			const Clause *c = new Clause(new BoolExpr(
						CmpInst::ICMP_EQ,
						new Expr(v1),
						new Expr(v2)));
			bool ret = provable(vector<const Clause *>(1, c));
			delete c;
			return ret;
		}

	private:
		// Intermediate VC expressions will be inserted to member <vc>. 
		VCExpr translate_to_vc(const Clause *c);
		VCExpr translate_to_vc(const BoolExpr *be);
		VCExpr translate_to_vc(const Expr *e);
		VCExpr translate_to_vc(const Value *v);
		VCExpr translate_to_vc(const Use *u);
		void avoid_overflow(unsigned op, VCExpr left, VCExpr right);

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

		VC vc;
	};
}

#endif
