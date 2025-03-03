/*
 * PCG Random Number Generation for C.
 *
 * MIT License
 *
 * Copyright (c) 2014-2017 Melissa O'Neill and PCG Project contributors
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _pcg_h_
#define _pcg_h_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if __cplusplus
extern "C"
{
#endif

struct pcg_state_setseq_64
{	// Internals are *Private*.
	uint64_t state;  // RNG state.  All values are possible.
	uint64_t inc;	// Controls which RNG sequence (stream) is
		// selected. Must *always* be odd.
};
typedef struct pcg_state_setseq_64 pcg32_random_t;

// If you *must* statically initialize it, here's one.

#define PCG32_INITIALIZER { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL }

// functions for seeding generators
bool pcg_entropy_getbytes(void* dest, size_t size);
void pcg_fallback_entropy_getbytes(void* dest, size_t size);

void pcg32_entropy_seed ();
void pcg32_entropy_seed_r (pcg32_random_t* rng);

// pcg32_srandom(initstate, initseq)
// pcg32_srandom_r(rng, initstate, initseq):
//     Seed the rng.  Specified in two parts, state initializer and a
//     sequence selection constant (a.k.a. stream id)

void pcg32_srandom (uint64_t initstate, uint64_t initseq);
void pcg32_srandom_r (pcg32_random_t* rng, uint64_t initstate, uint64_t initseq);

// pcg32_random()
// pcg32_random_r(rng)
//     Generate a uniformly distributed 32-bit random number

uint32_t pcg32_random (void);
uint32_t pcg32_random_r (pcg32_random_t* rng);

// pcg32_boundedrand(bound):
// pcg32_boundedrand_r(rng, bound):
//     Generate a uniformly distributed number, r, where 0 <= r < bound

uint32_t pcg32_boundedrand (uint32_t bound);
uint32_t pcg32_boundedrand_r (pcg32_random_t* rng, uint32_t bound);

void pcg32_advance (uint64_t delta);
void pcg32_advance_r (pcg32_random_t* rng, uint64_t delta);


#if !defined (PCG_NO_32X2)
/*
 * This code shows how you can cope if you're on a 32-bit platform (or a
 * 64-bit platform with a mediocre compiler) that doesn't support 128-bit math.
 *
 * Here we build a 64-bit generator by tying together two 32-bit generators.
 * Note that we can do this because we set up the generators so that each
 * 32-bit generator has a *totally different* different output sequence
 * -- if you tied together two identical generators, that wouldn't be nearly
 * as good.
 *
 * For simplicity, we keep the period fixed at 2^64.  The state space is
 * approximately 2^254 (actually  2^64 * 2^64 * 2^63 * (2^63 - 1)), which
 * is huge.
 */

#define PCG32x2_INITIALIZER {{ {0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL}, \
	{0x979c9a98d8462005ULL, 0x7d3e9cb6cfe0549bULL} }}

typedef struct {
	pcg32_random_t gen[2];
} pcg32x2_random_t;

void pcg32x2_entropy_seed ();
void pcg32x2_entropy_seed_r (pcg32x2_random_t* rng);

void pcg32x2_srandom(uint64_t seed1, uint64_t seed2, uint64_t seq1,  uint64_t seq2);
void pcg32x2_srandom_r(pcg32x2_random_t* rng, uint64_t seed1, uint64_t seed2, uint64_t seq1,  uint64_t seq2);

uint64_t pcg32x2_random();
uint64_t pcg32x2_random_r(pcg32x2_random_t* rng);

uint64_t pcg32x2_boundedrand(uint64_t bound);
uint64_t pcg32x2_boundedrand_r(pcg32x2_random_t* rng, uint64_t bound);

void pcg32x2_advance(uint64_t delta);
void pcg32x2_advance_r(pcg32x2_random_t* rng, uint64_t delta);

#endif  // end 32x2 declarations


// if using clang or gcc
// 	128bit integers should be present
#if !defined (__clang__) && !defined (__GNUC__)
#define PCG_NO_128BIT
#endif

// if you just dont want 64bit random numbers define PCG_NO_128BIT
#if !defined (PCG_NO_128BIT)

typedef __uint128_t pcg128_t;
#define PCG_128BIT_CONSTANT(high, low) ((((pcg128_t) high) << 64) + low)

struct pcg_state_setseq_128
{	// Internals are *Private*.
	pcg128_t state;  // RNG state.  All values are possible.
	pcg128_t inc;	// Controls which RNG sequence (stream) is
		// selected. Must *always* be odd.
};
typedef struct pcg_state_setseq_128 pcg64_random_t;

#define PCG64_INITIALIZER                                                 \
	{ PCG_128BIT_CONSTANT (0x979c9a98d8462005ULL, 0x7d3e9cb6cfe0549bULL),  \
	  PCG_128BIT_CONSTANT (0x0000000000000001ULL, 0xda3e39cb94b95bdbULL) }

void pcg64_entropy_seed ();
void pcg64_entropy_seed_r (pcg64_random_t* rng);

void pcg64_srandom (pcg128_t initstate, pcg128_t initseq);
void pcg64_srandom_r (pcg64_random_t* rng, pcg128_t initstate, pcg128_t initseq);

uint64_t pcg64_random (void);
uint64_t pcg64_random_r (pcg64_random_t* rng);

uint64_t pcg64_boundedrand (uint64_t bound);
uint64_t pcg64_boundedrand_r (pcg64_random_t* rng, uint64_t bound);

void pcg64_advance (pcg128_t delta);
void pcg64_advance_r (pcg64_random_t* rng, pcg128_t delta);

#endif

#if __cplusplus
}
#endif

#endif  // PCG_BASIC_H_INCLUDED
