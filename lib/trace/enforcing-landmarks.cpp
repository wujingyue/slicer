/**
 * Author: Jingyue
 */

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/CFG.h"
#include "rcs/util.h"
#include "rcs/IdentifyThreadFuncs.h"
using namespace llvm;

#include "slicer/enforcing-landmarks.h"
using namespace slicer;

#include <set>
#include <fstream>
using namespace std;

/*
 * Default enforcing landmark function names and whether they are blocking. 
 */
static const char *DEFAULT_ENFORCING_LANDMARK_FUNCS[][2] = {
	{"pthread_mutex_init",            "n"},
	{"pthread_mutex_lock",            "y"},
	{"pthread_mutex_unlock",          "n"},
	{"pthread_mutex_trylock",         "y"},
	{"pthread_mutex_destroy",         "n"},
	{"pthread_create",                "n"},
	{"pthread_join",                  "y"},
	{"pthread_exit",                  "n"},
	{"pthread_cond_init",             "n"},
	{"pthread_cond_wait",             "y"},
	{"pthread_cond_timedwait",        "y"},
	{"pthread_cond_broadcast",        "n"},
	{"pthread_cond_signal",           "n"},
	{"pthread_cond_destroy",          "n"},
	{"pthread_barrier_init",          "n"},
	{"pthread_barrier_wait",          "y"},
	{"pthread_barrier_destroy",       "n"},
	{"pthread_self",                  "n"},
	{"pthread_rwlock_init",           "n"},
	{"pthread_rwlock_destroy",        "n"},
	{"pthread_rwlock_wrlock",         "y"},
	{"pthread_rwlock_rdlock",         "y"},
	{"pthread_rwlock_trywrlock",      "y"},
	{"pthread_rwlock_tryrdlock",      "y"},
	{"pthread_rwlock_unlock",         "n"},
	{"accept",                        "y"},
	{"select",                        "y"},
	{"sigwait",                       "y"},
	{"sem_post",                      "n"},
	{"sem_wait",                      "y"},
	{"sem_trywait",                   "y"},
	{"sem_timedwait",                 "y"},
	{"epoll_wait",                    "y"},
	{"exit",                          "n"}
};

static RegisterPass<EnforcingLandmarks> X(
    "enforcing-landmarks",
		"Identify enforcing landmarks",
    false,
    true);

static cl::opt<string> EnforcingLandmarksFile(
    "input-landmarks",
		cl::desc("If this option is specified, MarkLandmarks uses the "
			"landmarks from the file as enforcing landmarks."));
static cl::opt<bool> OnlyMain(
    "only-main",
		cl::desc("Only mark the sync operations in main thread as "
			"enforcing landmarks."));
static cl::opt<int> PruningRate(
    "pruning-rate",
		cl::desc("Pruning rate of enforcing landmarks: pr/100 will be pruned."),
		cl::init(0));

char EnforcingLandmarks::ID = 0;

void EnforcingLandmarks::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
}

EnforcingLandmarks::EnforcingLandmarks(): ModulePass(ID) {}

bool EnforcingLandmarks::is_enforcing_landmark(const Instruction *ins) const {
	return enforcing_landmarks.count(const_cast<Instruction *>(ins));
}

bool EnforcingLandmarks::is_blocking_enforcing_landmark(
		const Instruction *ins) const {
	if (!is_enforcing_landmark(ins))
		return false;
	CallSite cs(const_cast<Instruction *>(ins));
	if (!cs.getInstruction())
		return false;
	Function *callee = cs.getCalledFunction();
	return callee && blocking_enforcing_landmark_funcs.count(callee->getName());
}

const InstSet &EnforcingLandmarks::get_enforcing_landmarks() const {
	return enforcing_landmarks;
}

bool EnforcingLandmarks::runOnModule(Module &M) {
	if (EnforcingLandmarksFile == "") {
		const static size_t len = sizeof(DEFAULT_ENFORCING_LANDMARK_FUNCS) /
			sizeof(const char *[2]);
		for (size_t i = 0; i < len; ++i) {
			string func_name = DEFAULT_ENFORCING_LANDMARK_FUNCS[i][0];
			string is_blocking = DEFAULT_ENFORCING_LANDMARK_FUNCS[i][1];
			insert_enforcing_landmark_func(func_name, is_blocking);
		}
	} else {
		ifstream fin(EnforcingLandmarksFile.c_str());
		string func_name, is_blocking;
		while (fin >> func_name >> is_blocking)
			insert_enforcing_landmark_func(func_name, is_blocking);
	}

	// Mark any function call to landmark functions as enforcing landmarks. 
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		if (f->getName() == "MyMalloc")
			continue;
		if (f->getName() == "MyFree")
			continue;
		if (f->getName() == "MyFreeNow")
			continue;
		if (f->getName() == "maketree")
			continue;
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				CallSite cs(ins);
				if (cs.getInstruction()) {
					if (Function *callee = cs.getCalledFunction()) {
						StringRef callee_name = callee->getName();
						if (enforcing_landmark_funcs.count(callee_name)) {
							if (OnlyMain && callee_name != "pthread_self" &&
									f->getName() != "main")
								continue;
							if (callee_name.startswith("pthread_mutex")
									|| callee_name.startswith("pthread_cond")
									|| callee_name.startswith("pthread_barrier")
									|| callee_name.startswith("pthread_rwlock")
									|| callee_name.startswith("pthread_spin")) {
								if (rand() % 100 < PruningRate)
									continue;
							}
							enforcing_landmarks.insert(ins);
						}
					}
				}
			}
		}
	}

	return false;
}

void EnforcingLandmarks::insert_enforcing_landmark_func(const string &func_name,
		const string &is_blocking) {
	enforcing_landmark_funcs.insert(func_name);
	if (is_blocking == "y" || is_blocking == "Y")
		blocking_enforcing_landmark_funcs.insert(func_name);
}
