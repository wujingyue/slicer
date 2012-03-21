#include <pthread.h>

#include <iostream>
#include <cassert>
#include <cstdlib>
#include <vector>
using namespace std;

#define THREADS_SHUTDOWN (0)
#define THREADS_CMD_NEWPARTICLES (1)
#define THREADS_IDLE (2)
#define WORKUNIT_SIZE_PARTICLEWEIGHTS (4)

string path;
int cameras, frames, particles, layers, threads;
int *valid;

struct TicketDispenser {
	void init(int _inc) {
		value = 0;
		inc = _inc;
		pthread_mutex_init(&l, NULL);
	}

	int getTicket() {
		int rv;
		pthread_mutex_lock(&l);
		rv = value;
		value += inc;
		pthread_mutex_unlock(&l);
		return rv;
	}

	pthread_mutex_t l;
	int value, inc;
};

struct WorkerGroup;

void *thread_entry(WorkerGroup *arg);

struct WorkerGroup {
	__attribute__((always_inline)) void Init(int nThreads) {
		pthread_mutex_init(&workDispatch, NULL);
		pthread_cond_init(&workAvailable, NULL);
		pthread_barrier_init(&workDoneBarrier, NULL, nThreads);
		pthread_barrier_init(&poolReadyBarrier, NULL, nThreads + 1);
		particleTickets.init(WORKUNIT_SIZE_PARTICLEWEIGHTS);
		cmd = THREADS_IDLE;
		children.clear();
		for (int i = 0; i < nThreads; ++i) {
			pthread_t child;
			pthread_create(&child, NULL, (void *(*)(void *))thread_entry, this);
			children.push_back(child);
		}
	}

	__attribute__((always_inline)) void Run() {
		bool doExit = false;
		while (!doExit) {
			int cmd = RecvCmd();
			if (cmd == THREADS_SHUTDOWN) {
				doExit = true;
			} else if (cmd == THREADS_IDLE) {
				assert(false);
			} else {
				Exec(cmd);
			}
			AckCmd();
		}
	}

	__attribute__((always_inline)) int RecvCmd() {
		pthread_mutex_lock(&workDispatch);
		while (cmd == THREADS_IDLE)
			pthread_cond_wait(&workAvailable, &workDispatch);
		int _cmd = cmd;
		pthread_mutex_unlock(&workDispatch);
		return _cmd;
	}

	__attribute__((always_inline)) void Exec(int cmd) {
		if (cmd == THREADS_CMD_NEWPARTICLES) {
			int ticket = particleTickets.getTicket();
			while (ticket < particles) {
				cerr << "ticket = " << ticket << "\n";
				for (int i = ticket; i < particles &&
						i < ticket + WORKUNIT_SIZE_PARTICLEWEIGHTS; ++i) {
					valid[i] = rand();
				}
				ticket = particleTickets.getTicket();
			}
		}
	}

	__attribute__((always_inline)) void AckCmd() {
		pthread_barrier_wait(&workDoneBarrier);
		pthread_mutex_lock(&workDispatch);
		cmd = THREADS_IDLE;
		pthread_mutex_unlock(&workDispatch);
		pthread_barrier_wait(&poolReadyBarrier);
	}

	__attribute__((always_inline)) void SendCmd(int _cmd) {
		pthread_mutex_lock(&workDispatch);
		cmd = _cmd;
		pthread_cond_broadcast(&workAvailable);
		pthread_mutex_unlock(&workDispatch);

		pthread_barrier_wait(&poolReadyBarrier);
	}

	__attribute__((always_inline)) void JoinAll() {
		SendCmd(THREADS_SHUTDOWN);
		for (size_t i = 0; i < children.size(); ++i)
			pthread_join(children[i], NULL);
	}

	pthread_mutex_t workDispatch;
	pthread_cond_t workAvailable;
	pthread_barrier_t workDoneBarrier, poolReadyBarrier;
	TicketDispenser particleTickets;
	vector<pthread_t> children;
	int cmd;
};

void *thread_entry(WorkerGroup *arg) {
	WorkerGroup *wg = (WorkerGroup *)arg;
	wg->Run();
	return NULL;
}

WorkerGroup workers;

__attribute__((always_inline)) void GenerateNewParticles(int k) {
	workers.SendCmd(THREADS_CMD_NEWPARTICLES);
}

__attribute__((always_inline)) void update(float timeval) {
	for (int k = layers - 1; k >= 0; --k) {
		GenerateNewParticles(k);
	}
}

__attribute__((always_inline)) int mainPthreads() {
	for (int i = 0; i < frames; ++i) {
		update((float)i);
	}
	workers.JoinAll();
	return 0;
}

int main(int argc, char *argv[]) {
	if (argc != 7) {
		cerr << "Usage: <path> <cameras> <frames> <particles> <layers> <threads>\n";
		return 1;
	}

	path = argv[1];
	cameras = atoi(argv[2]);
	frames = atoi(argv[3]);
	particles = atoi(argv[4]);
	layers = atoi(argv[5]);
	threads = atoi(argv[6]);
	valid = new int[particles];
	workers.Init(threads);
	return mainPthreads();
}
