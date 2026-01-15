//prime.h
#ifndef PRIME_H
#define PRIME_H


int is_prime_sqrt(int n);
int is_prime_trail(int n);
int is_prime_wheel(int n);

int *generate_prime_with_sieve(int n);
int *generate_prime_with_sieve_simd(int n);


#endif // PRIME_H