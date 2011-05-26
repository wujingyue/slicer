#ifndef __SLICER_SOLVER_H
#define __SLICER_SOLVER_H

#include "expression.h"

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
		// Remember to add CaptureConstraints transitively. 
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;

		bool satisfiable(const vector<Clause *> &more_clauses);

	private:
	};
}

#endif
