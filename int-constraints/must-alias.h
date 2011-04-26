#ifndef __SLICER_MUST_ALIAS_H
#define __SLICER_MUST_ALIAS_H

// TODO: fit into AliasAnalysis interface. 

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "common/include/typedefs.h"
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
		~MustAlias();
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

		bool must_alias(const Value *v1, const Value *v2) const;
		bool must_alias(
				vector<User *> *ctxt1, const Value *v1,
				vector<User *> *ctxt2, const Value *v2) const;
		// Context-insensitive. 
		// Use the alias sets. 
		bool fast_must_alias(const Value *v1, const Value *v2) const;
		// Returns NULL if <v> is not even a candidate. 
		// In that case, it's not even aliasing with itself, because
		// it may point to multiple dynamic locations. 
		const ConstValueSet *get_alias_set(const Value *v) const;

	private:
		// Called by <get_all_candidates>.
		void get_all_pointers(
				const Module &M, ConstValueList &pointers) const;
		void get_all_candidates(
				const Module &M, ConstValueList &candidates) const;
		bool get_single_pointee(
				vector<User *> *ctxt, const Value *v,
				PointeeType &ptt, const Value *&pt) const;
		static void print_value(raw_ostream &O, const Value *v);

		// root[v] means the representative of v's containing set. 
		ConstValueMapping root;
		DenseMap<const Value *, ConstValueSet *> alias_sets;
	};
}

#endif
