/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *       http://www.pcg-random.org
 */

/*
 * This code is derived from the full C implementation, which is in turn
 * derived from the canonical C++ PCG implementation. The C++ version
 * has many additional features and is preferable if you can use C++ in
 * your project.
 */

#include <stdint.h>
#include <time.h>

#include "pcg.h"

#ifndef IS_UNIX
#if !defined(_WIN32) && (defined(__unix__)  || defined(__unix) \
						 || (defined(__APPLE__) && defined(__MACH__)))
	#define IS_UNIX 1
#else
	#define IS_UNIX 0
#endif
#endif

/* If HAVE_DEV_RANDOM is set, we use that value, otherwise we guess */
#ifndef HAVE_DEV_RANDOM
#define HAVE_DEV_RANDOM		 IS_UNIX
#endif

#if HAVE_DEV_RANDOM
	#include <fcntl.h>
	#include <unistd.h>
#endif

#ifndef __has_include
	#define INCLUDE_OKAY(x) 1
#else
	#define INCLUDE_OKAY(x) __has_include(x)
#endif

#if __STDC_VERSION__ >= 201112L && !__STDC_NO_ATOMICS__ \
		&& INCLUDE_OKAY(<stdatomic.h>)
	#include <stdatomic.h>
	#define PCG_SPINLOCK_DECLARE(mutex) atomic_flag mutex = ATOMIC_FLAG_INIT
	#define PCG_SPINLOCK_LOCK(mutex)	do {} \
										while (atomic_flag_test_and_set(&mutex))
	#define PCG_SPINLOCK_UNLOCK(mutex)  atomic_flag_clear(&mutex)
#elif __GNUC__
	#define PCG_SPINLOCK_DECLARE(mutex) volatile int mutex = 0
	#define PCG_SPINLOCK_LOCK(mutex)	\
				do {} while(__sync_lock_test_and_set(&mutex, 1))
	#define PCG_SPINLOCK_UNLOCK(mutex)  __sync_lock_release(&mutex)
#else
	#warning No implementation of spinlocks provided.  No thread safety.
	#define PCG_SPINLOCK_DECLARE(mutex) volatile int mutex = 0
	#define PCG_SPINLOCK_LOCK(mutex)	\
				do { while(mutex == 1){} mutex = 1; } while(0)
	#define PCG_SPINLOCK_UNLOCK(mutex)  mutex = 0;
#endif

#define PCG_DEFAULT_MULTIPLIER_64 6364136223846793005ULL

// state for global RNGs
static pcg32_random_t pcg32_global = PCG32_INITIALIZER;

#if HAVE_DEV_RANDOM 
/* pcg_entropy_getbytes(dest, size):
 *     Use /dev/urandom to get some external entropy for seeding purposes.
 *
 * Note:
 *     If reading /dev/urandom fails (which ought to never happen), it returns
 *     false, otherwise it returns true.  If it fails, you could instead call
 *     fallback_entropy_getbytes which always succeeds.
 */

bool pcg_entropy_getbytes(void* dest, size_t size)
{
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0)
	{
		return false;
	}
	ssize_t sz = read(fd, dest, size);
	return (close(fd) == 0) && (sz == (ssize_t) size);
}
#else
bool pcg_entropy_getbytes(void* dest, size_t size)
{
	fallback_entropy_getbytes(dest, size);
	return true;
}
#endif

/* pcg_fallback_entropy_getbytes(dest, size):
 *	 Works like the /dev/random version above, but avoids using /dev/random.
 *	 Instead, it uses a private RNG (so that repeated calls will return
 *	 different seeds).  Makes no attempt at cryptographic security.
 */

void pcg_fallback_entropy_getbytes(void* dest, size_t size)
{
	/* Most modern OSs use address-space randomization, meaning that we can
	   use the address of stack variables and system library code as
	   initializers.  It's not as good as using /dev/random, but probably
	   better than using the current time alone. */

	static PCG_SPINLOCK_DECLARE(mutex);
	PCG_SPINLOCK_LOCK(mutex);

	static int intitialized = 0;
	static pcg32_random_t entropy_rng;
	
	if (!intitialized) {
		int dummyvar;
		pcg32_srandom_r(&entropy_rng,
						time(NULL) ^ (intptr_t)&pcg_fallback_entropy_getbytes, 
						(intptr_t)&dummyvar);
		intitialized = 1;
	}
	
	char* dest_cp = (char*) dest;
	for (size_t i = 0; i < size; ++i) {
		dest_cp[i] = (char) pcg32_random_r(&entropy_rng);
	}

	PCG_SPINLOCK_UNLOCK(mutex);
}

// pcg32_srandom(initstate, initseq)
// pcg32_srandom_r(rng, initstate, initseq):
//     Seed the rng.  Specified in two parts, state initializer and a
//     sequence selection constant (a.k.a. stream id)

void pcg32_srandom_r (pcg32_random_t* rng, uint64_t initstate, uint64_t initseq)
{
	rng->state = 0U;
	rng->inc = (initseq << 1u) | 1u;
	pcg32_random_r (rng);
	rng->state += initstate;
	pcg32_random_r (rng);
}

void pcg32_srandom (uint64_t seed, uint64_t seq)
{
	pcg32_srandom_r (&pcg32_global, seed, seq);
}

// pcg32_random()
// pcg32_random_r(rng)
//     Generate a uniformly distributed 32-bit random number

uint32_t pcg32_random_r (pcg32_random_t* rng)
{
	uint64_t oldstate = rng->state;
	rng->state = oldstate * PCG_DEFAULT_MULTIPLIER_64 + rng->inc;
	uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
	uint32_t rot = oldstate >> 59u;
	return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

uint32_t pcg32_random ()
{
	return pcg32_random_r (&pcg32_global);
}

// pcg32_boundedrand(bound):
// pcg32_boundedrand_r(rng, bound):
//     Generate a uniformly distributed number, r, where 0 <= r < bound

uint32_t pcg32_boundedrand_r (pcg32_random_t* rng, uint32_t bound)
{
	// To avoid bias, we need to make the range of the RNG a multiple of
	// bound, which we do by dropping output less than a threshold.
	// A naive scheme to calculate the threshold would be to do
	//
	//     uint32_t threshold = 0x100000000ull % bound;
	//
	// but 64-bit div/mod is slower than 32-bit div/mod (especially on
	// 32-bit platforms).  In essence, we do
	//
	//     uint32_t threshold = (0x100000000ull-bound) % bound;
	//
	// because this version will calculate the same modulus, but the LHS
	// value is less than 2^32.

	uint32_t threshold = -bound % bound;

	// Uniformity guarantees that this loop will terminate.  In practice, it
	// should usually terminate quickly; on average (assuming all bounds are
	// equally likely), 82.25% of the time, we can expect it to require just
	// one iteration.  In the worst case, someone passes a bound of 2^31 + 1
	// (i.e., 2147483649), which invalidates almost 50% of the range.  In
	// practice, bounds are typically small and only a tiny amount of the range
	// is eliminated.
	for (;;)
	{
		uint32_t r = pcg32_random_r (rng);
		if (r >= threshold)
			return r % bound;
	}
}

uint32_t pcg32_boundedrand (uint32_t bound)
{
	return pcg32_boundedrand_r (&pcg32_global, bound);
}

void pcg32_advance (uint64_t delta)
{
	pcg32_advance_r (&pcg32_global, delta);
}

/* Multi-step advance functions (jump-ahead, jump-back)
 *
 * The method used here is based on Brown, "Random Number Generation
 * with Arbitrary Stride,", Transactions of the American Nuclear
 * Society (Nov. 1994).  The algorithm is very similar to fast
 * exponentiation.
 *
 * Even though delta is an unsigned integer, we can pass a
 * signed integer to go backwards, it just goes "the long way round".
 */
void pcg32_advance_r (pcg32_random_t* rng, uint64_t delta)
{
	uint64_t cur_mult = PCG_DEFAULT_MULTIPLIER_64;
	uint64_t cur_plus = rng->inc;
	uint64_t acc_mult = 1u;
	uint64_t acc_plus = 0u;

	while (delta > 0)
	{
		if (delta & 1)
		{
			acc_mult *= PCG_DEFAULT_MULTIPLIER_64;
			acc_plus = acc_plus * cur_mult + cur_plus;
		}

		cur_plus = (cur_mult + 1) * cur_plus;
		cur_mult *= cur_mult;
		delta /= 2;
	}

	rng->state = acc_mult * rng->state + acc_plus;
}

// seed the global generator using entropy function
void pcg32_entropy_seed ()
{
	pcg32_entropy_seed_r (&pcg32_global);
}

void pcg32_entropy_seed_r (pcg32_random_t* rng)
{
	uint64_t pcg_seed;
	uint64_t pcg_sequence;

	if (!pcg_entropy_getbytes (&pcg_seed, sizeof (pcg_seed)))
	{
		pcg_fallback_entropy_getbytes (&pcg_seed, sizeof (pcg_seed));
	}
	if (!pcg_entropy_getbytes (&pcg_sequence, sizeof (pcg_sequence)))
	{
		pcg_fallback_entropy_getbytes (&pcg_sequence, sizeof (pcg_sequence));
	}

	pcg32_srandom_r (rng, pcg_seed, pcg_sequence);
}

#if !defined(PCG_NO_32X2)

static pcg32x2_random_t pcg32x2_global = PCG32x2_INITIALIZER;

void pcg32x2_srandom(uint64_t seed1, uint64_t seed2, uint64_t seq1,  uint64_t seq2)
{
	pcg32x2_srandom_r (&pcg32x2_global, seed1, seed2, seq1, seq2);
}

void pcg32x2_srandom_r(pcg32x2_random_t* rng, uint64_t seed1, uint64_t seed2, uint64_t seq1,  uint64_t seq2)
{
	uint64_t mask = ~0ull >> 1;
	/* The stream for each of the two generators *must* be distinct */
	if ((seq1 & mask) == (seq2 & mask)) 
		seq2 = ~seq2;
	pcg32_srandom_r(rng->gen,   seed1, seq1);
	pcg32_srandom_r(rng->gen+1, seed2, seq2);
}

uint64_t pcg32x2_random()
{
	return pcg32x2_random_r (&pcg32x2_global);
}

uint64_t pcg32x2_random_r(pcg32x2_random_t* rng)
{
	return ((uint64_t)(pcg32_random_r(rng->gen)) << 32)
		   | pcg32_random_r(rng->gen+1);
}

void pcg32x2_advance(uint64_t delta)
{
	pcg32x2_advance_r (&pcg32x2_global, delta);
}

void pcg32x2_advance_r(pcg32x2_random_t* rng, uint64_t delta)
{
	pcg32_advance_r(rng->gen, delta);
	pcg32_advance_r(rng->gen + 1, delta);
}

/* See other definitons of ..._boundedrand_r for an explanation of this code. */

uint64_t pcg32x2_boundedrand(uint64_t bound)
{
	return pcg32x2_boundedrand_r (&pcg32x2_global, bound);
}

uint64_t pcg32x2_boundedrand_r(pcg32x2_random_t* rng, uint64_t bound)
{
	uint64_t threshold = -bound % bound;
	for (;;) {
		uint64_t r = pcg32x2_random_r(rng);
		if (r >= threshold)
			return r % bound;
	}
}

void pcg32x2_entropy_seed ()
{
	pcg32x2_entropy_seed_r (&pcg32x2_global);
}

void pcg32x2_entropy_seed_r (pcg32x2_random_t* rng)
{
	uint64_t seeds[4];

	pcg_entropy_getbytes ((void*) seeds, sizeof (seeds));
	pcg32x2_srandom_r (rng, seeds[0], seeds[1], seeds[2], seeds[3]);
}

#endif  // end 32x2 definitions

#if !defined (PCG_NO_128BIT)

#define PCG_DEFAULT_MULTIPLIER_128 \
		PCG_128BIT_CONSTANT(2549297995355413924ULL,4865540595714422341ULL)

static pcg64_random_t pcg64_global = PCG64_INITIALIZER;

void pcg64_srandom_r (pcg64_random_t* rng, pcg128_t initstate, pcg128_t initseq)
{
	rng->state = 0U;
	rng->inc = (initseq << 1u) | 1u;
	pcg64_random_r (rng);
	rng->state += initstate;
	pcg64_random_r (rng);
}

void pcg64_srandom (pcg128_t seed, pcg128_t seq)
{
	pcg64_srandom_r (&pcg64_global, seed, seq);
}

uint64_t pcg64_random_r (pcg64_random_t* rng)
{
	// original xor shift version
	//pcg128_t oldstate = rng->state;
	//rng->state = oldstate * PCG_DEFAULT_MULTIPLIER_128 + rng->inc;
	//uint64_t xorshifted = ((uint64_t)(oldstate >> 64u) ^ (uint64_t)oldstate);
	//uint64_t rot = oldstate >> 122u;
	//return (xorshifted >> rot) | (xorshifted << ((-rot) & 63));

	// dxsm - double xorshift multiply
	// from: https://github.com/imneme/pcg-cpp/commit/871d0494ee9c9a7b7c43f753e3d8ca47c26f8005
	uint64_t half_width_multiplier = 15750249268501108917ULL;
	pcg128_t state = rng->state;
	rng->state = state * half_width_multiplier + rng->inc;
	uint64_t hi = (uint64_t) (state >> 64);
	uint64_t lo = (uint64_t) (state | 1);
	hi ^= hi >> 32;
	hi *= half_width_multiplier;
	hi ^= hi >> 48;  // 3 * (64 / 4)
	hi *= lo;

	return hi;
}

uint64_t pcg64_random ()
{
	return pcg64_random_r (&pcg64_global);
}

uint64_t pcg64_boundedrand_r (pcg64_random_t* rng, uint64_t bound)
{
	uint64_t threshold = -bound % bound;

	for (;;)
	{
		uint64_t r = pcg64_random_r (rng);
		if (r >= threshold)
			return r % bound;
	}
}

uint64_t pcg64_boundedrand (uint64_t bound)
{
	return pcg64_boundedrand_r (&pcg64_global, bound);
}

void pcg64_advance (pcg128_t delta)
{
	pcg64_advance_r (&pcg64_global, delta);
}

void pcg64_advance_r (pcg64_random_t* rng, pcg128_t delta)
{
	pcg128_t cur_mult = PCG_DEFAULT_MULTIPLIER_128;
	pcg128_t cur_plus = rng->inc;
	pcg128_t acc_mult = 1u;
	pcg128_t acc_plus = 0u;

	while (delta > 0)
	{
		if (delta & 1)
		{
			acc_mult *= cur_mult;
			acc_plus = acc_plus * cur_mult + cur_plus;
		}

		cur_plus = (cur_mult + 1) * cur_plus;
		cur_mult *= cur_mult;
		delta /= 2;
	}

	rng->state = acc_mult * rng->state + acc_plus;
}

void pcg64_entropy_seed ()
{
	pcg64_entropy_seed_r (&pcg64_global);
}

void pcg64_entropy_seed_r (pcg64_random_t* rng)
{
	pcg128_t pcg_seed;
	pcg128_t pcg_sequence;

	if (!pcg_entropy_getbytes (&pcg_seed, sizeof (pcg_seed)))
	{
		pcg_fallback_entropy_getbytes (&pcg_seed, sizeof (pcg_seed));
	}
	if (!pcg_entropy_getbytes (&pcg_sequence, sizeof (pcg_sequence)))
	{
		pcg_fallback_entropy_getbytes (&pcg_sequence, sizeof (pcg_sequence));
	}

	pcg64_srandom_r (rng, pcg_seed, pcg_sequence);
}

#endif
