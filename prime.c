// prime.c
#include <stdio.h>
#include <stdlib.h>
#include "prime.h"

int is_prime_trail(int n)
{
    if (n == 1)
    {
        return 0;
    }

    for (int i = 2; i <= n / 2; i++)
    {
        if (n % i == 0)
        {
            return 0;
        }
    }

    return 1;
}

int is_prime_sqrt(int n)
{
    if (n == 1)
    {
        return 0;
    }

    for (int i = 2; i * i <= n; i++)
    {
        if (n % i == 0)
        {
            return 0;
        }
    }

    return 1;
}



int is_prime_wheel(int n)
{
    if (n <= 1)
    {
        return 0;
    }

    if (n == 2 || n == 3 || n == 5)
    {
        return 1;
    }

    if (n % 2 == 0 || n % 3 == 0 || n % 5 == 0)
    {
        return 0;
    }

    int residues[] = {1, 7, 11, 13, 17, 19, 23, 29};
    int base = 30;

    for (int k = 0;; k++)
    {
        for (int i = 0; i < 8; i++)
        {
            int d = base * k + residues[i];

            if (d < 7)
            {
                continue;
            }

            if (d * d > n)
            {
                return 1;
            }

            if (n % d == 0)
            {
                return 0;
            }
        }
    }
}

int *generate_prime_with_sieve(int n)
{
    int *prime = calloc((size_t)n + 1, sizeof(int));
    if (!prime)
        return NULL;

    if (n >= 0) prime[0] = 1;
    if (n >= 1) prime[1] = 1;

    for (int i = 2; (long long)i * i <= n; i++)
    {
        if (prime[i] == 0)
        {
            for (int j = i * i; j <= n; j += i)
                prime[j] = 1;
        }
    }

    return prime;
}
