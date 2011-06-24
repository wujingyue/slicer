/**
 * Author: Jingyue
 *
 * TODO: make it configurable. e.g. Read from a config file. 
 */

#ifndef __SLICER_LANDMARKS_H
#define __SLICER_LANDMARKS_H

#include "llvm/Support/CallSite.h"
#include "common/include/util.h"
using namespace llvm;

#include <string>
using namespace std;

namespace slicer {

	/*
	 * There are two types of landmarks: application landmarks and
	 * derived landmarks. 
	 */
	const static char *APP_LANDMARKS[] = {
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

	inline bool is_app_landmark(const string &func_name) {
		const size_t len = sizeof(APP_LANDMARKS) / sizeof(APP_LANDMARKS[0]);
		for (size_t i = 0; i < len; ++i) {
			if (func_name == APP_LANDMARKS[i])
				return true;
		}
		return false;
	}

	inline bool is_app_landmark(Instruction *ins) {
		if (!is_call(ins))
			return false;
		Function *callee = CallSite(ins).getCalledFunction();
		if (!callee)
			return false;
		return is_app_landmark(callee->getNameStr());
	}
}

#endif
