#include "llvm/Support/CFG.h"
#include "llvm/Support/CommandLine.h"
#include "idm/id.h"
#include "common/cfg/identify-thread-funcs.h"
using namespace llvm;

#include <fstream>
#include <sstream>
#include <set>
using namespace std;

#include "mark-landmarks.h"
#include "omit-branch.h"
using namespace slicer;

/*
 * Default enforcing landmark function names. 
 */
static const char *DEFAULT_ENFORCING_LANDMARK_FUNCS[] = {
	"pthread_mutex_init",
	"pthread_mutex_lock",
	"pthread_mutex_unlock",
	"pthread_mutex_trylock",
	"pthread_mutex_destroy",
	"pthread_create",
	"pthread_join",
	"pthread_exit",
	"pthread_cond_init",
	"pthread_cond_wait",
	"pthread_cond_timedwait",
	"pthread_cond_broadcast",
	"pthread_cond_signal",
	"pthread_cond_destroy",
	"pthread_barrier_init",
	"pthread_barrier_wait",
	"pthread_barrier_destroy",
	"pthread_self",
	"pthread_rwlock_init",
	"pthread_rwlock_destroy",
	"pthread_rwlock_wrlock",
	"pthread_rwlock_rdlock",
	"pthread_rwlock_trywrlock",
	"pthread_rwlock_tryrdlock",
	"pthread_rwlock_unlock",
	"sleep",
	"usleep",
	"nanosleep",
	"accept",
	"select",
	"sigwait",
	"sem_post",
	"sem_wait",
	"sem_trywait",
	"sem_timedwait",
	"epoll_wait",
	"exit"
};

static RegisterPass<MarkLandmarks> X(
		"mark-landmarks",
		"Mark landmarks",
		false,
		true); // is analysis
static cl::opt<string> EnforcingLandmarksFile(
		"input-landmarks",
		cl::desc("If this option is specified, MarkLandmarks uses the "
			"landmarks from the file as enforcing landmarks."),
		cl::init(""));

char MarkLandmarks::ID = 0;

bool MarkLandmarks::runOnModule(Module &M) {
	landmarks.clear();
	enforcing_landmarks.clear();
	mark_enforcing_landmarks(M);
	mark_branch_succs(M);
	mark_thread(M);
	return false;
}

void MarkLandmarks::mark_enforcing_landmarks(Module &M) {

	set<string> enforcing_landmark_funcs;
	if (EnforcingLandmarksFile == "") {
		size_t len = sizeof(DEFAULT_ENFORCING_LANDMARK_FUNCS) / sizeof(char *);
		for (size_t i = 0; i < len; ++i)
			enforcing_landmark_funcs.insert(DEFAULT_ENFORCING_LANDMARK_FUNCS[i]);
	} else {
		ifstream fin(EnforcingLandmarksFile.c_str());
		string func;
		while (fin >> func)
			enforcing_landmark_funcs.insert(func);
	}

	forallinst(M, ii) {
		CallSite cs = CallSite::get(ii);
		if (cs.getInstruction()) {
			Function *callee = cs.getCalledFunction();
			if (callee && enforcing_landmark_funcs.count(callee->getName())) {
				landmarks.insert(ii);
				enforcing_landmarks.insert(ii);
			}
		}
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

const InstSet &MarkLandmarks::get_landmarks() const {
	return landmarks;
}

const InstSet &MarkLandmarks::get_enforcing_landmarks() const {
	return enforcing_landmarks;
}
