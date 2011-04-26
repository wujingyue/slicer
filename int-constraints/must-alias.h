#ifndef __SLICER_MUST_ALIAS_H
#define __SLICER_MUST_ALIAS_H

// TODO: fit into AliasAnalysis interface. 

#include "llvm/Module.h"
#include "llvm/Pass.h"
using namespace llvm;

#include <vector>
using namespace std;

namespace slicer {
	
	struct MustAlias: public ModulePass {

		static char ID;

		enum PointeeType {
			GLOBAL_VAR,
			STACK_LOC,
			HEAP_LOC
		};

		MustAlias(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

		bool must_alias(const Value *v1, const Value *v2) const;
		bool must_alias(
				vector<User *> *ctxt1, const Value *v1,
				vector<User *> *ctxt2, const Value *v2) const;

	private:
		void get_all_pointers(
				const Module &M, vector<const Value *> &pointers) const;
		bool get_single_pointee(
				vector<User *> *ctxt, const Value *v,
				PointeeType &ptt, const Value *&pt) const;
		static void print_value(raw_ostream &O, const Value *v);
	};
}

#endif
