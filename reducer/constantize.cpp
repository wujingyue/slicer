/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "constantize"

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

void Constantize::getAnalysisUsage(Analysis &AU) const {
	AU.addRequired<Iterate>();
	AU.addRequired<CaptureConstraints>();
	AU.addRequired<SolveConstraints>();
	ModulePass::getAnalysisUsage(AU);
}

bool Constantize::runOnModule(Module &M) {
	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	return false;
}
