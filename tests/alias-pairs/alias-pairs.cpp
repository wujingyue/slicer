#include "llvm/Pass.h"
#include "llvm/Function.h"

#include "common/id.h"
#include "common/util.h"
#include "common/typedefs.h"
using namespace llvm;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

namespace slicer {
	struct AliasPairs: public ModulePass {
		static char ID;

		AliasPairs(): ModulePass(&ID) {}
		
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

	private:
		void print_inst(Instruction *ins, raw_ostream &O) const;
		vector<InstPair> ww_races, rw_races;
	};
}

char AliasPairs::ID = 0;

static RegisterPass<slicer::AliasPairs> X("alias-pairs",
		"An LLVM pass dumping all alias pairs",
		false, true);

void AliasPairs::print_inst(Instruction *ins, raw_ostream &O) const {
	BasicBlock *bb = ins->getParent();
	Function *func = bb->getParent();
	O << func->getNameStr() << ":" << bb->getNameStr() << ":\t";
	ins->print(O);
	O << "\n";
}

void AliasPairs::print(raw_ostream &O, const Module *M) const {
	O << "\nWrite-write potential races:\n";
	forallconst(vector<InstPair>, it, ww_races) {
		O << string(10, '=') << "\n";
		print_inst(it->first, O);
		print_inst(it->second, O);
	}
	O << "\nRead-write potential races:\n";
	forallconst(vector<InstPair>, it, rw_races) {
		O << string(10, '=') << "\n";
		print_inst(it->first, O);
		print_inst(it->second, O);
	}
}

bool AliasPairs::runOnModule(Module &M) {
	InstList rd_insts, wr_insts;
	forallinst(M, ii) {
		if (isa<StoreInst>(ii))
			wr_insts.push_back(ii);
		if (isa<LoadInst>(ii))
			rd_insts.push_back(ii);
	}

	BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();

	for (size_t i = 0; i < wr_insts.size(); ++i) {
		for (size_t j = 0; j < wr_insts.size(); ++j) {
			if (BAA.alias(
						dyn_cast<StoreInst>(wr_insts[i])->getPointerOperand(),
						0,
						dyn_cast<StoreInst>(wr_insts[j])->getPointerOperand(),
						0)) {
				ww_races.push_back(make_pair(wr_insts[i], wr_insts[j]));
			}
		}
	}

	for (size_t i = 0; i < wr_insts.size(); ++i) {
		for (size_t j = 0; j < rd_insts.size(); ++j) {
			if (BAA.alias(
						dyn_cast<StoreInst>(wr_insts[i])->getPointerOperand(),
						0,
						dyn_cast<LoadInst>(rd_insts[j])->getPointerOperand(),
						0)) {
				rw_races.push_back(make_pair(wr_insts[i], rd_insts[j]));
			}
		}
	}

	return false;
}

void AliasPairs::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<BddAliasAnalysis>();
	ModulePass::getAnalysisUsage(AU);
}
