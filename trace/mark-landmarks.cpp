#include "llvm/Support/CFG.h"
#include "llvm/Support/CommandLine.h"
#include "idm/id.h"
#include "common/cfg/identify-thread-funcs.h"
using namespace llvm;

#include <fstream>
#include <sstream>
using namespace std;

#include "mark-landmarks.h"
#include "omit-branch.h"
#include "slicer-landmarks.h"

namespace {
	
	static RegisterPass<slicer::MarkLandmarks> X(
			"mark-landmarks",
			"Mark landmarks",
			false,
			true); // is analysis
	static cl::opt<string> CutFile(
			"cut",
			cl::desc("If the cut file is specified, MarkLandmark gets the "
				"landmarks from the file instead of computing them"),
			cl::init(""));
}

namespace slicer {

	bool MarkLandmarks::runOnModule(Module &M) {
		landmarks.clear();
		if (CutFile == "") {
			mark_enforcing_landmarks(M);
			mark_branch_succs(M);
			mark_thread(M);
		} else {
			read_landmarks(CutFile);
		}
		return false;
	}

	void MarkLandmarks::read_landmarks(const string &cut_file) {
		ifstream fin(cut_file.c_str());
		assert(fin && "Cannot open the specified cut file");

		ObjectID &IDM = getAnalysis<ObjectID>();
		string line;
		while (getline(fin, line)) {
			istringstream iss(line);
			unsigned ins_id;
			iss >> ins_id;
			Instruction *ins = IDM.getInstruction(ins_id);
			assert(ins && "Cannot find the specified instruction ID");
			landmarks.insert(ins);
		}
	}

	void MarkLandmarks::mark_enforcing_landmarks(Module &M) {
		forallinst(M, ii) {
			if (is_app_landmark(ii))
				landmarks.insert(ii);
		}
	}

	void MarkLandmarks::mark_thread(Module &M) {
		IdentifyThreadFuncs &ITF = getAnalysis<IdentifyThreadFuncs>();
		forallfunc(M, fi) {
			if (fi->isDeclaration())
				continue;
			if (ITF.is_thread_func(fi) || is_main(fi)) {
				Instruction *first = fi->getEntryBlock().getFirstNonPHI();
				landmarks.insert(first);
				forall(Function, bi, *fi) {
					if (succ_begin(bi) == succ_end(bi))
						landmarks.insert(bi->getTerminator());
				}
			}
		}
	}

	Instruction *MarkLandmarks::get_first_non_intrinsic(
			Instruction *start) const {
		BasicBlock::iterator first = start;
		BasicBlock::iterator end = first->getParent()->end();
		while (first != end && is_intrinsic_call(first))
			++first;
		if (first != end)
			return first;
		else
			assert(false);
	}

	void MarkLandmarks::mark_branch_succs(Module &M) {
		OmitBranch &OB = getAnalysis<OmitBranch>();
		forallinst(M, ii) {
			BranchInst *bi = dyn_cast<BranchInst>(ii);
			if (bi && !OB.omit(bi)) {
				for (unsigned i = 0; i < bi->getNumSuccessors(); ++i) {
					BasicBlock *succ = bi->getSuccessor(i);
					landmarks.insert(succ->getFirstNonPHI());
				}
			}
		}
	}

	void MarkLandmarks::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequiredTransitive<ObjectID>();
		AU.addRequired<OmitBranch>();
		AU.addRequired<IdentifyThreadFuncs>();
		ModulePass::getAnalysisUsage(AU);
	}

	void MarkLandmarks::print(raw_ostream &O, const Module *M) const {
		ObjectID &IDM = getAnalysis<ObjectID>();
		vector<unsigned> all_inst_ids;
		forallconst(InstSet, it, landmarks) {
			unsigned ins_id = IDM.getInstructionID(*it);
			assert(ins_id != ObjectID::INVALID_ID);
			all_inst_ids.push_back(ins_id);
		}
		sort(all_inst_ids.begin(), all_inst_ids.end());
		for (size_t i = 0; i < all_inst_ids.size(); ++i)
			O << all_inst_ids[i] << "\n";
	}

	bool MarkLandmarks::is_landmark(Instruction *ins) const {
		return landmarks.count(ins);
	}

	const InstSet &MarkLandmarks::get_landmarks() const {
		return landmarks;
	}

	char MarkLandmarks::ID = 0;
}

