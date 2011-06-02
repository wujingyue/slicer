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

		VC vc;
	};
}

#endif
