#include <stdlib.h>
#include "simd.h"
#include "prime.h"

bool simd_supported(void)
{
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    return true;
#elif defined(__AVX2__) || defined(__SSE2__)
    return true;
#else
    return false;
#endif
}

int *generate_prime_with_sieve_simd(int n)
{
#if defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__AVX2__) || defined(__SSE2__)
    return generate_prime_with_sieve(n);
#else
    return generate_prime_with_sieve(n);
#endif
}
