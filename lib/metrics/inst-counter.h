/**
 * Author: Jingyue
 */

#ifndef __SLICER_INST_COUNTER_H
#define __SLICER_INST_COUNTER_H

#include "llvm/Pass.h"
using namespace llvm;

namespace slicer {
	// TODO: Make it a InstVisitor. 
	struct InstCounter: public ModulePass {
		static char ID;

		InstCounter(): ModulePass(ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;
		
	private:
		void count_inst(const Instruction *ins, unsigned ins_id);

		DenseSet<unsigned> counted_ins_ids;
	};
}

#endif
