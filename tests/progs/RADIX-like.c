#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX_P (1024)
#define PAGE_SIZE (4096)
#define MAX_RADIX (4096)
#define DEFAULT_M (524288)

struct GlobalMemory {
	int id;
	pthread_mutex_t id_lock;
	pthread_barrier_t barrier_rank;
} *global;

struct GlobalPrivate {
	int *rank_ff;
} gp[MAX_P];

int p;
int radix;
int max_key;
int **rank_me;
int max_num_digits;
int log2_radix;

void *slave_sort(void *arg) {
	int local_id;
	int *rank_me_mynum;
	int *rank_ff_mynum;
	int loopnum;
	int i;

	pthread_mutex_lock(&global->id_lock);
	local_id = global->id;
	++global->id;
	pthread_mutex_unlock(&global->id_lock);

	rank_me_mynum = rank_me[local_id];
	rank_ff_mynum = gp[local_id].rank_ff;

	for (i = 0; i < radix; ++i)
		rank_ff_mynum[i] = 0;

	for (loopnum = 0; loopnum < max_num_digits; ++loopnum) {
		int shiftnum;
		int bb;
		int my_key;

		shiftnum = loopnum * log2_radix;
		bb = (radix - 1) << shiftnum;
		pthread_barrier_wait(&global->barrier_rank);
		my_key = rand() & bb;
		my_key = my_key >> shiftnum;
		printf("loopnum = %d; my_key = %d\n", loopnum, my_key);
		rank_me_mynum[my_key]++;
	}

	return NULL;
}

int get_max_digits(int max_key) {
	int done = 0;
	int temp = 1;
	int key_val;

	key_val = max_key;
	while (!done) {
		key_val = key_val / radix;
		if (key_val == 0) {
			done = 1;
		} else {
			temp ++;
		}
	}
	return temp;
}

long log_2(long number) {
	long cumulative = 1;
	long out = 0;
	long done = 0;

	while ((cumulative < number) && (!done) && (out < 50)) {
		if (cumulative == number) {
			done = 1;
		} else {
			cumulative = cumulative * 2;
			out ++;
		}
	}

	if (cumulative == number) {
		return(out);
	} else {
		return(-1);
	}
}

int main(int argc, char *argv[]) {
	int i;
	int size;
	int **temp;
	int **temp2;
	int *a;
	int rem;
	pthread_t children[MAX_P];

	if (argc < 4) {
		fprintf(stderr, "Usage: %s <# of processors> <radix> <max_key>\n", argv[0]);
		return 1;
	}
	p = atoi(argv[1]);
	radix = atoi(argv[2]);
	max_key = atoi(argv[3]);
	log2_radix = log_2(radix);
	max_num_digits = get_max_digits(max_key);

	if (radix < 1 || radix > MAX_RADIX)
		exit(-1);
	if (max_key < 1 || max_key > DEFAULT_M)
		exit(-1);
	if (log2_radix < 0 || radix != (1 << log2_radix))
		exit(-1);
	rem = (max_key >> ((max_num_digits - 1) * log2_radix));
	if (max_num_digits <= 0 || max_num_digits >= 32 || rem <= 0 || rem >= radix)
		exit(-1);

	global = (struct GlobalMemory *)valloc(sizeof(struct GlobalMemory));
	pthread_mutex_init(&global->id_lock, NULL);
	pthread_barrier_init(&global->barrier_rank, NULL, p);
	global->id = 0;
	size = p * (radix * sizeof(int) + sizeof(int *));
	rank_me = (int **)valloc(size);

	// Initialize rank_ff
	for (i = 0; i < p; ++i)
		gp[i].rank_ff = (int *)valloc(radix * sizeof(int) + PAGE_SIZE);

	// Initialize rank_me
	temp = rank_me;
	temp2 = temp + p;
	a = (int *)temp2;
	for (i = 0; i < p; ++i) {
		*temp = (int *)a;
		++temp;
		a += radix;
	}

	for (i = 0; i < p - 1; ++i)
		pthread_create(&children[i], NULL, slave_sort, NULL);

	slave_sort(NULL);

	for (i = 0; i < p - 1; ++i)
		pthread_join(children[i], NULL);

	return 0;
}
