#ifndef __SLICER_INSTRUMENT_H
#define __SLICER_INSTRUMENT_H

#include "llvm/Pass.h"
using namespace llvm;

namespace slicer {
	struct Instrument: public ModulePass {
		static char ID;

		Instrument();
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);

	private:
		void setup(Module &M);
		bool should_instrument(Instruction *ins) const;

		Type *uint_type, *bool_type;
		Function *init_trace, *trace_inst, *pth_create_wrapper;
	};
}

#endif
