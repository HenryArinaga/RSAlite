#include <stdlib.h>
#include <stdint.h>
#include "prime.h"
#include "simd.h"

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define RSALITE_NEON 1
#elif defined(__SSE2__)
#include <emmintrin.h>
#define RSALITE_SSE2 1
#endif

int *generate_prime_with_sieve_simd(int n)
{
    int *prime = calloc((size_t)n + 1, sizeof(int));
    if (!prime)
        return NULL;

    if (n >= 0) prime[0] = 1;
    if (n >= 1) prime[1] = 1;

    for (int i = 2; (int64_t)i * i <= n; i++)
    {
        if (prime[i])
            continue;

        int j = i * i;

#if RSALITE_NEON
        int step = i * 4;
        int32x4_t ones = vdupq_n_s32(1);

        for (; j + 3 * i <= n; j += step)
        {
            prime[j] = 1;
            prime[j + i] = 1;
            prime[j + 2 * i] = 1;
            prime[j + 3 * i] = 1;
        }

#elif RSALITE_SSE2
        int step = i * 4;

        for (; j + 3 * i <= n; j += step)
        {
            prime[j] = 1;
            prime[j + i] = 1;
            prime[j + 2 * i] = 1;
            prime[j + 3 * i] = 1;
        }
#endif

        for (; j <= n; j += i)
            prime[j] = 1;
    }

    return prime;
}
