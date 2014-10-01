#ifndef __SLICER_QUERY_TRANSLATOR_H
#define __SLICER_QUERY_TRANSLATOR_H

#include "llvm/Pass.h"
#include "rcs/typedefs.h"

namespace slicer {
	struct DynamicInsID {
		int thread_id;
		size_t trunk_id;
		unsigned ins_id;
	};

	struct QueryTranslator: public ModulePass {
		static char ID;
		
		QueryTranslator();
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;

	private:
		bool parse_raw_query(const string &line,
				vector<DynamicInsID> &a, vector<DynamicInsID> &b);
		void print_contexted_ins(ostream &O, const vector<unsigned> &a);
		void print_query(ostream &O,
				const vector<unsigned> &a, const vector<unsigned> &b);
		void translate_contexted_ins(const vector<DynamicInsID> &a,
				vector<unsigned> &a2);
	};
}

#endif
