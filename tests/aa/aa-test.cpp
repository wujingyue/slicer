/**
 * Author: Jingyue
 */

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Support/CommandLine.h"
#include "common/include/util.h"
#include "common/include/typedefs.h"
#include "idm/id.h"
using namespace llvm;

#include <map>
#include <vector>
using namespace std;

namespace slicer {

	struct AATest: public ModulePass {

		static char ID;

		AATest(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	};
}
using namespace slicer;

static RegisterPass<AATest> X(
		"aa-test",
		"Test the integer constraint solver",
		false,
		false);

char AATest::ID = 0;

void AATest::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<AliasAnalysis>();
	ModulePass::getAnalysisUsage(AU);
}

bool AATest::runOnModule(Module &M) {
	
	vector<const Value *> store_to, load_from;
	forallinst(M, ii) {
		if (StoreInst *si = dyn_cast<StoreInst>(ii)) {
			store_to.push_back(si->getPointerOperand());
		} else if (LoadInst *li = dyn_cast<LoadInst>(ii)) {
			load_from.push_back(li->getPointerOperand());
		}
	}
	
	unsigned n_alias = 0;
	vector<ConstValuePair> alias;
	for (size_t i = 0; i < store_to.size(); ++i) {
		for (size_t j = 0; j < load_from.size(); ++j) {
			AliasAnalysis &AA = getAnalysis<AliasAnalysis>();
			if (AA.alias(store_to[i], 0, load_from[j], 0) != AliasAnalysis::NoAlias) {
				alias.push_back(make_pair(store_to[i], load_from[j]));
				n_alias++;
			}
		}
	}
	for (size_t k = 0; k < alias.size(); ++k) {
		if (alias[k].first > alias[k].second)
			swap(alias[k].first, alias[k].second);
	}
	sort(alias.begin(), alias.end());
	alias.resize(unique(alias.begin(), alias.end()) - alias.begin());

	errs() << "# of aliasing pairs = " << n_alias << "\n";
	errs() << "# of distinct aliasing pairs = " << alias.size() << "\n";

	return false;
}
