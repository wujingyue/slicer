/**
 * Author: Jingyue
 */

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/CFG.h"
#include "common/util.h"
#include "common/identify-thread-funcs.h"
using namespace llvm;

#include "slicer/enforcing-landmarks.h"
using namespace slicer;

#include <set>
#include <fstream>
using namespace std;

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

static RegisterPass<EnforcingLandmarks> X("enforcing-landmarks",
		"Identify enforcing landmarks",
		false, true); // is analysis

static cl::opt<string> EnforcingLandmarksFile("input-landmarks",
		cl::desc("If this option is specified, MarkLandmarks uses the "
			"landmarks from the file as enforcing landmarks."),
		cl::init(""));

char EnforcingLandmarks::ID = 0;

void EnforcingLandmarks::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	ModulePass::getAnalysisUsage(AU);
}

bool EnforcingLandmarks::is_enforcing_landmark(const Instruction *ins) const {
	return enforcing_landmarks.count(const_cast<Instruction *>(ins));
}

const InstSet &EnforcingLandmarks::get_enforcing_landmarks() const {
	return enforcing_landmarks;
}

bool EnforcingLandmarks::runOnModule(Module &M) {
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

	// Mark any function call to landmark functions as enforcing landmarks. 
	forallfunc(M, f) {
		if (f->getName() == "MyMalloc")
			continue;
		if (f->getName() == "MyFree")
			continue;
		if (f->getName() == "MyFreeNow")
			continue;
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				CallSite cs = CallSite::get(ins);
				if (cs.getInstruction()) {
					Function *callee = cs.getCalledFunction();
					if (callee && enforcing_landmark_funcs.count(callee->getName()))
						enforcing_landmarks.insert(ins);
				}
			}
		}
	}

	return false;
}
