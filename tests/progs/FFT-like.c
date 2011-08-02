#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX_P (1024)

struct GlobalMemory {
	long id;
	pthread_mutex_t id_lock;
} *global;

void *SlaveStart(void *arg) {
	
	int local_id;

	pthread_mutex_lock(&global->id_lock);
	local_id = global->id;
	++global->id;
	pthread_mutex_unlock(&global->id_lock);

	fprintf(stderr, "local_id = %d\n", local_id);

	return NULL;
}

int main(int argc, char *argv[]) {

	int p, i;
	pthread_t children[MAX_P];

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <# of processors>\n", argv[0]);
		return 1;
	}

	global = (struct GlobalMemory *)malloc(sizeof(struct GlobalMemory));
	pthread_mutex_init(&global->id_lock, NULL);
	global->id = 0;

	p = atoi(argv[1]);
	for (i = 0; i < p - 1; ++i)
		pthread_create(&children[i], NULL, SlaveStart, NULL);

	SlaveStart(NULL);

	for (i = 0; i < p - 1; ++i)
		pthread_join(children[i], NULL);
	
	return 0;
}
