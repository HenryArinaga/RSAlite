#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "factor.h"
#include "prime.h"
#include "optimization.h"
#include "simd.h"

static inline bool cancel_requested(const struct OptimizationContext *opt)
{
    return opt && opt->cancel_flag && *opt->cancel_flag;
}
int factor_with_trial_simd(uint64_t n,
                           uint64_t *factors,
                           int max_factors,
                           const struct OptimizationContext *opt)
{
    return factor_with_trial(n, factors, max_factors, false, opt);
}

int factor_with_trial(uint64_t n,
                      uint64_t *factors,
                      int max_factors,
                      bool use_sieve,
                      const struct OptimizationContext *opt)
{

    if (use_sieve && opt && opt->USE_SIMD && simd_supported())
    {
        return factor_with_trial_simd(n, factors, max_factors, opt);
    }

    int count = 0;

    if (n <= 1)
        return 0;

    int *prime = NULL;

    uint64_t limit = 0;
    for (limit = 1; (limit + 1) <= n / (limit + 1); limit++)
    {
    }

    if (use_sieve)
    {
        if (opt && opt->USE_SIMD && simd_supported())
            prime = generate_prime_with_sieve_simd((int)limit);
        else
            prime = generate_prime_with_sieve((int)limit);
        if (!prime)
            return 0;
    }

    for (uint64_t p = 2; p <= n / p; p++)
    {
        if (cancel_requested(opt))
        {
            free(prime);
            return count;
        }

        if (use_sieve)
        {
            if (p <= limit && prime[p])
                continue;
        }
        else
        {
            if (!is_prime_trail(p))
                continue;
        }

        if (n % p == 0)
        {
            if (count < max_factors)
                factors[count++] = p;

            while (n % p == 0)
            {
                if (cancel_requested(opt))
                {
                    free(prime);
                    return count;
                }
                n /= p;
            }
        }
    }

    if (n > 1 && count < max_factors)
        factors[count++] = n;

    free(prime);
    return count;
}

int factor_with_sqrt(uint64_t n,
                     uint64_t *factors,
                     int max_factors,
                     const struct OptimizationContext *opt)
{
    int count = 0;

    for (uint64_t p = 2; p <= n / p; p++)
    {
        if (cancel_requested(opt))
            return count;

        if (is_prime_sqrt(p) && n % p == 0)
        {
            if (count < max_factors)
                factors[count++] = p;

            while (n % p == 0)
            {
                if (cancel_requested(opt))
                    return count;
                n /= p;
            }
        }
    }

    if (cancel_requested(opt))
        return count;

    if (n > 1 && count < max_factors)
        factors[count++] = n;

    return count;
}

int factor_with_wheel(uint64_t n,
                      uint64_t *factors,
                      int max_factors,
                      const struct OptimizationContext *opt)
{
    int count = 0;

    if (n <= 1)
        return 0;

    uint64_t small_primes[] = {2, 3, 5};

    for (int i = 0; i < 3; i++)
    {
        if (cancel_requested(opt))
            return count;

        uint64_t p = small_primes[i];
        if (n % p == 0)
        {
            if (count < max_factors)
                factors[count++] = p;

            while (n % p == 0)
            {
                if (cancel_requested(opt))
                    return count;
                n /= p;
            }
        }
    }

    if (n == 1)
        return count;

    int residues[] = {1, 7, 11, 13, 17, 19, 23, 29};
    int base = 30;

    for (uint64_t k = 0;; k++)
    {
        if (cancel_requested(opt))
            return count;

        for (int i = 0; i < 8; i++)
        {
            if (cancel_requested(opt))
                return count;

            uint64_t d = (uint64_t)base * k + (uint64_t)residues[i];

            if (d <= 1)
                continue;

            if (d > n / d)
                goto done;

            if (n % d == 0)
            {
                if (count < max_factors)
                    factors[count++] = d;

                while (n % d == 0)
                {
                    if (cancel_requested(opt))
                        return count;
                    n /= d;
                }
            }
        }
    }

done:
    if (cancel_requested(opt))
        return count;

    if (n > 1 && count < max_factors)
        factors[count++] = n;

    return count;
}

int factor_with_fermat(uint64_t n,
                       uint64_t *factors,
                       int max_factors,
                       const struct OptimizationContext *opt)
{
    return factor_with_sqrt(n, factors, max_factors, opt);
}

int factor_with_pollard(uint64_t n,
                        uint64_t *factors,
                        int max_factors,
                        const struct OptimizationContext *opt)
{
    return factor_with_sqrt(n, factors, max_factors, opt);
}



int factor_number(uint64_t n,
                  FactorMethod method,
                  uint64_t *factors,
                  int max_factors,
                  const struct OptimizationContext *opt)
{
    static const struct OptimizationContext default_opt = {
        .USE_SIEVE = false,
        .USE_SIMD = false,
        .USE_MULTITHREADING = false,
        .USE_GPU = false,
        .USE_BENCHMARKING = false,
        .cancel_flag = NULL};

    if (opt == NULL)
        opt = &default_opt;

    switch (method)
    {
    case FACTOR_METHOD_TRIAL:
        return factor_with_trial(n, factors, max_factors, opt->USE_SIEVE, opt);

    case FACTOR_METHOD_SQRT:
        return factor_with_sqrt(n, factors, max_factors, opt);

    case FACTOR_METHOD_WHEEL:
        return factor_with_wheel(n, factors, max_factors, opt);

    case FACTOR_METHOD_FERMAT:
        return factor_with_fermat(n, factors, max_factors, opt);

    case FACTOR_METHOD_POLLARD:
        return factor_with_pollard(n, factors, max_factors, opt);

    default:
        return 0;
    }
}
