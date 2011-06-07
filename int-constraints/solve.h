/**
 * Author: Jingyue
 */

// TODO: Supports 32-bit signed integers only. 

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
		bool may_equal(const Value *v1, const Value *v2);
		bool must_equal(const Value *v1, const Value *v2);

	private:
		// Intermediate VC expressions will be inserted to member <vc>. 
		VCExpr translate_to_vc(const Clause *c);
		VCExpr translate_to_vc(const BoolExpr *be);
		VCExpr translate_to_vc(const Expr *e);
		VCExpr translate_to_vc(const Value *v);
		VCExpr translate_to_vc(const Use *u);

		static void vc_error_handler(const char *err_msg);

		/**
		 * The place the value is used may give us extra constraints. 
		 * This function is designed to capture those constraints. 
		 * It looks at the intra-procedural control flow,
		 * and adds branch conditions that need to be satisfied along the way. 
		 * TODO: inter-procedural
		 */
		void realize_uses(const Clause *c);
		void realize_uses(const BoolExpr *c);
		void realize_uses(const Expr *c);
		void realize_use(const Use *u);

		VC vc;
	};
}

#endif
