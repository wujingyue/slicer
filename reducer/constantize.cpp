/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "constantize"

#include "llvm/ADT/Statistic.h"
using namespace llvm;

#include "../int/iterate.h"
#include "../int/capture.h"
#include "../int/solve.h"
#include "constantize.h"
using namespace slicer;

static RegisterPass<Constantize> X(
		"constantize",
		"Replace variables with constant values whenever possible "
		"according to the results from the integer constraint solver",
		false,
		false);

STATISTIC(VariablesConstantized, "Number of variables constantized");

char Constantize::ID = 0;

void Constantize::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<Iterate>();
	AU.addRequired<CaptureConstraints>();
	AU.addRequired<SolveConstraints>();
	ModulePass::getAnalysisUsage(AU);
}

bool Constantize::runOnModule(Module &M) {

	errs() << "Constantize::runOnModule\n";
	
	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();

	vector<pair<const Value *, ConstantInt *> > to_replace;
	const ConstValueSet &constants = CC.get_constants();
	forallconst(ConstValueSet, it, constants) {
		// Skip if already a constant. 
		if (isa<Constant>(*it))
			continue;
		assert(isa<Instruction>(*it) || isa<Argument>(*it));
		if (ConstantInt *ci = SC.get_fixed_value(*it))
			to_replace.push_back(make_pair(*it, ci));
	}

	bool changed = false;
	for (size_t i = 0; i < to_replace.size(); ++i) {
		const Value *v = to_replace[i].first;
		vector<Use *> local;
		// Don't replace uses while iterating. 
		// Put them to a local list first. 
		for (Value::use_const_iterator ui = v->use_begin();
				ui != v->use_begin(); ++ui)
			local.push_back(&ui.getUse());
		if (local.size() > 0) {
			++VariablesConstantized;
			errs() << "=== replacing with a constant ===\n";
		}
		for (size_t j = 0; j < local.size(); ++j) {
			local[j]->set(to_replace[i].second);
			changed = true;
		}
	}

	return changed;
}
