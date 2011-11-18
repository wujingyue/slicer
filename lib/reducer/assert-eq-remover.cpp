/**
 * Author: Jingyue
 */

#include "slicer/InitializePasses.h"
using namespace llvm;

#include "slicer/capture.h"
#include "slicer/assert-eq-remover.h"
using namespace slicer;

INITIALIZE_PASS(AssertEqRemover, "remove-assert-eq",
		"Remove redundant slicer_assert_eq function calls", false, false)

char AssertEqRemover::ID = 0;

AssertEqRemover::AssertEqRemover(): BasicBlockPass(ID) {
	initializeAssertEqRemoverPass(*PassRegistry::getPassRegistry());
}

bool AssertEqRemover::runOnBasicBlock(BasicBlock &BB) {
	bool changed = false;

	BasicBlock::iterator ins = BB.begin();
	while (ins != BB.end()) {
		const Value *v = NULL;
		if (CaptureConstraints::is_slicer_assert_eq(ins, &v, NULL)) {
			if (isa<Constant>(v)) {
				BasicBlock::iterator next = ins; ++next;
				ins->eraseFromParent();
				ins = next;
				changed = true;
				continue;
			}
		}
		++ins;
	}

	return changed;
}
