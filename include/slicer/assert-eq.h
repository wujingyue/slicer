#ifndef __SLICER_ASSERT_EQ_H
#define __SLICER_ASSERT_EQ_H

/**
 * Our integer constraint solver supports 32-bit integers only, therefore
 * <unsigned> is enough. 
 *
 * Mark it as extern "C" so that we could use it to annotate both C and
 * C++ programs. 
 */
extern "C" __attribute__((noinline))
void slicer_assert_eq(unsigned a, unsigned b) {
	assert(a == b);
}

#endif
