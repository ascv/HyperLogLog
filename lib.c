#include "loglog.h"

/*
 * Returns a cardinality estimate of the words in file using
 * 2^k buckets and the specified murmur3 seed.
 */
double hyperLogLog(FILE * file, uint32_t k, uint32_t seed) {

  char str [100];
  uint32_t * hash = (uint32_t *) malloc(sizeof(uint32_t));
  uint32_t index;
  uint32_t size = 1 << k;
  uint32_t rank;
  uint32_t ranks [size];

  uint32_t i;
  for (i = 0; i < size; i++) {
    ranks[i] = 0;
  }

  while ((fscanf(file, "%100s", str)) == 1) {
    MurmurHash3_x86_32((void *) str, strlen(str), 42, (void *) hash);
    index = *hash >> (32 - k);
    rank = leadingZeroCount((*hash << k) >> k) - k + 1;

    if (rank > ranks[index]) {
      ranks[index] = rank;
    }
  }

  double alpha = 0.0;
  switch (size) {
      case 16:
	alpha = 0.673;
	break;
      case 32:
	alpha = 0.697;
	break;
      case 64:
	alpha = 0.709;
	break;
      default:
	alpha = 0.7213/(1.0 + 1.079/(double) size);
        break;
  }
  
  double sum = 0.0;
  for (i = 0; i < size; i++) {
    sum = sum + 1.0/pow(2, (double) ranks[i]);
  }

  double j = (double) 0x7FFFFFFF; // 2^32
  double estimate = alpha * pow(sum, -1) * pow(size, 2);
  uint32_t * intEstimate = (uint32_t *) &estimate;

  if (estimate <= 2.5 * size) {
    uint32_t zeros = 32 - ones(*intEstimate);
    if (zeros != 0) {
      estimate = size * log(size/zeros);
    }
  }
  if (estimate <= (1.0/3.0) * j) {
    estimate = estimate;
  }
  else if (estimate > (1.0/3.0) * j) {
    estimate = (-1.0 * j) * log(1.0 - estimate/j);
  }

  free(hash);
  return estimate;
}

/* 
 * Get the number of leading zeros in x.
 */
uint32_t leadingZeroCount(uint32_t x) {
  x |= (x >> 1);
  x |= (x >> 2);
  x |= (x >> 4);
  x |= (x >> 8);
  x |= (x >> 16);
  return (32 - ones(x));
}

/*
 * Get the number of bits set to 1 in x.
 */
uint32_t ones(uint32_t x) {
  x -= (x >> 1) & 0x55555555;
  x = ((x >> 2) & 0x33333333) + (x & 0x33333333);
  x = ((x >> 4) + x) & 0x0F0F0F0F;
  x += (x >> 8);
  x += (x >> 16);
  return(x & 0x0000003F);
}
