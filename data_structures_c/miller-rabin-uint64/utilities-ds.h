/**
   utilities-ds.h

   Declarations of accessible utility functions across the areas of 
   randomness, modular arithmetic, and binary representation.

   Update: 6/23/2020 11:00am
*/

#ifndef UTILITIES_DS_H  
#define UTILITIES_DS_H

#include <stdint.h>
#include <stdbool.h>

/**
   Returns a generator-uniform random uint64_t.
*/
uint64_t random_uint64();

/**
   Returns a generator-uniform random uint32_t. 
*/
uint32_t random_uint32();

/**
   Returns a generator-uniform random uint64_t in [0 , n].
*/
uint64_t random_range_uint64(uint64_t n);

/**
   Returns a generator-uniform random uint32_t in [0 , n].
*/
uint32_t random_range_uint32(uint32_t n);

/**
   Given a threshold in [low, high], where high > low, returns true with 
   probability (threshold - low)/(high - low). 
*/
bool bern_uint64(uint64_t threshold, uint64_t low, uint64_t high);

/**
   Given a threshold in [low, high], where high > low, returns true with 
   probability (threshold - low)/(high - low). 
*/
bool bern_uint32(uint32_t threshold, uint32_t low, uint32_t high);

/**
   Computes overflow-safe unsigned mod n of the kth power.
*/
uint64_t pow_mod_uint64(uint64_t a, uint64_t k, uint64_t n);

/**
   Computes overflow-safe unsigned mod n of the kth power.
*/
uint32_t pow_mod_uint32(uint32_t a, uint64_t k, uint32_t n);

/**
   Computes overflow-safe unsigned (a * b) mod n.
*/
uint64_t mul_mod_uint64(uint64_t a, uint64_t b, uint64_t n);

/**
   Computes overflow-safe unsigned (a + b) mod n.
*/
uint64_t sum_mod_uint64(uint64_t a, uint64_t b, uint64_t n);

/**
   Computes mod n of a memory block, treating each byte of the block in the 
   little-endian order. Does not require a little-endian machine.
*/
uint32_t mem_mod_uint32(void *s, uint64_t size, uint32_t n);

/**
   Represents n as u * 2^k, where u is odd.
*/
void represent_uint64(uint64_t n, int *k, uint64_t *u);

/**
   Returns the kth power of 2, where 0 <= k <= 63.
*/
uint64_t pow_two_uint64(int k);

#endif