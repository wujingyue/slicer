#include <stdio.h>

#define SIZE (128)

struct Frame {
	int *pixels[SIZE][SIZE];
};

struct ThreadInputArg;

struct ThreadInput {
	struct Frame pic;
	pthread_t tid;
	int next_frame;
	int frame_total;
	struct ThreadInputArg *next_args;
};

struct ThreadInputArg {
	struct ThreadInput *h;
	struct Frame *pic;
	int i_frame;
};

void allocate_frame(struct Frame *frame) {
	frame->pixels = (int *)malloc(sizeof(int) * SIZE * SIZE);
}

void *encode_frame_int(struct Frame *frame) {
	int i, j;
	int result = 0;
	for (i = 0; i < SIZE; ++i) {
		for (j = 0; j < SIZE; ++j)
			result += frame->pixels[i][j];
	}
	printf("result = %d\n", result);
	return NULL;
}

void encode_frame(struct Frame *frame) {
	encode_frame_int(frame);
}

int read_frame_thread(struct Frame *frame, void *handle, int i) {
	struct ThreadInput *h = handle;

	if (h->next_frame >= 0) {
		void *status;
		pthread_join(h->tid, &status);
	}
	
	if (h->next_frame == i) {
		struct Frame *tmp = *frame;
		*frame = h->pic;
		h->pic = *tmp;
	} else {
		read_frame_thread_int(frame, i);
	}
	
	if (i + 1 < h->frame_total) {
		h->next_frame = i = 1;
		pthread_
}

struct ThreadInput ti;

int main(int argc, char *argv[]) {
	int i_frame;
	struct Frame pic;

	assert(argc > 1);

	open_file_thread(argc, argv, &ti);

	allocate_frame(&pic);
	for (i_fame = 0; i_frame < ti->frame_total; ++i_frame) {
		if (read_frame_thread(&pic, &ti, i_frame))
			break;
		encode_frame(&ti, &pic);
	}
	
	return 0;
}
