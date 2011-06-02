#ifndef __SLICER_ITERATE_H
#define __SLICER_ITERATE_H

namespace slicer {

	struct Iterate: public ModulePass {

		static char ID;

		Iterate(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;

	private:
		void run_tests(Module &M);
		void test1(Module &M);
		void test2(Module &M);
		void test3(Module &M);
	};
}

#endif
