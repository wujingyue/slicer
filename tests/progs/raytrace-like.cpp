#include <iostream>
#include <cassert>
#include <cstdlib>
#include <malloc.h>
using namespace std;

const int PACKET_WIDTH = 8;
const int SIMD_WIDTH = 4;
const int SIMD_VECTORS_PER_PACKET = PACKET_WIDTH * PACKET_WIDTH / SIMD_WIDTH;
const int TILE_WIDTH = PACKET_WIDTH * 4;

typedef struct {
	int data[4];
} __m128i;

pthread_mutex_t g_mutexCounter = PTHREAD_MUTEX_INITIALIZER;
int g_counter, g_maxTiles, g_resX, g_resY, g_nThreads;

static inline void _mm_stream_ps(float *a, __m128i b) {
	*(__m128i *)a = b;
}

struct FrameBuffer {
	FrameBuffer(int w, int h) __attribute__((always_inline)) {
		width = w;
		height = h;
		fb = (unsigned char *)memalign(64, 4 * w * h);
	}

	~FrameBuffer() {
		assert(fb);
		free(fb);
	}

	void writeBlock(int x0, int y0, int dx, int dy,
			const __m128i *four4x8PixelsEach) __attribute__((always_inline)) {
		assert(dx == 8 && dy == 8);
		unsigned int *const fb_as_int32 = (unsigned int *)fb;
		unsigned int *start = (unsigned int *)&fb_as_int32[y0 * width + x0];
		for (int y = 0; y < 8; ++y, start += width) {
			_mm_stream_ps((float *)&start[0], four4x8PixelsEach[y * 2 + 0]);
			_mm_stream_ps((float *)&start[4], four4x8PixelsEach[y * 2 + 1]);
		}
	}

private:
	unsigned char *fb;
	int width, height;
};

FrameBuffer *g_frameBuffer;

__attribute__((always_inline)) void randomFill(__m128i *arr, int n) {
	for (int i = 0; i < n; ++i) {
		for (int j = 0; j < 4; ++j)
			arr[i].data[j] = rand();
	}
}

__attribute__((always_inline)) void renderTile(FrameBuffer *frameBuffer,
		int startX, int startY, int endX, int endY) {
	fprintf(stderr, "renderTile: %d %d %d %d\n", startX, startY, endX, endY);
	__m128i rgb32[SIMD_VECTORS_PER_PACKET];
	randomFill(rgb32, SIMD_VECTORS_PER_PACKET);
#if 0
	for (int y = startY; y + PACKET_WIDTH <= endY; y += PACKET_WIDTH) {
		for (int x = startX; x + PACKET_WIDTH <= endX; x += PACKET_WIDTH) {
			frameBuffer->writeBlock(x, y, PACKET_WIDTH, PACKET_WIDTH, rgb32);
		}
	}
#endif
	for (int iy = 0; iy < (endY - startY) / PACKET_WIDTH; ++iy) {
		int y = startY + iy * PACKET_WIDTH;
		for (int ix = 0; ix < (endX - startX) / PACKET_WIDTH; ++ix) {
			int x = startX + ix * PACKET_WIDTH;
			frameBuffer->writeBlock(x, y, PACKET_WIDTH, PACKET_WIDTH, rgb32);
		}
	}
}

void *task(void *arg) {
	const int tilesPerRow = g_resX / TILE_WIDTH;

	while (true) {
		pthread_mutex_lock(&g_mutexCounter);
		int index = g_counter;
		++g_counter;
		pthread_mutex_unlock(&g_mutexCounter);
		if (index >= g_maxTiles)
			break;

		int sx = (index % tilesPerRow) * TILE_WIDTH;
		int sy = (index / tilesPerRow) * TILE_WIDTH;
		int ex = min(sx + TILE_WIDTH, g_resX);
		int ey = min(sy + TILE_WIDTH, g_resY);
		renderTile(g_frameBuffer, sx, sy, ex, ey);
	}

	return NULL;
}

int main(int argc, char *argv[]) {
	assert(argc > 3);
	g_resX = atoi(argv[1]);
	g_resY = atoi(argv[2]);
	g_nThreads = atoi(argv[3]);

	assert(g_resX % TILE_WIDTH == 0 && g_resY % TILE_WIDTH == 0);
	assert(g_resX <= 1024 && g_resY <= 1024);

	g_counter = 0;
	g_maxTiles = (g_resX / TILE_WIDTH) * (g_resY / TILE_WIDTH);
	g_frameBuffer = new FrameBuffer(g_resX, g_resY);

	pthread_t *children = new pthread_t[g_nThreads];
	for (int threadId = 0; threadId < g_nThreads; ++threadId)
		pthread_create(&children[threadId], NULL, task, (void *)threadId);
	for (int threadId = 0; threadId < g_nThreads; ++threadId)
		pthread_join(children[threadId], NULL);
	delete[] children;

	delete g_frameBuffer;

	return 0;
}
