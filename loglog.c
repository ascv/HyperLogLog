#include "loglog.h"

int main(int argc, char ** argv) {

  char *optString = "s:k:f:";
  FILE * fp = NULL;
  uint32_t k = -1;
  uint32_t murmur3Seed = 314;
  uint32_t opt;

  opt = getopt(argc, argv, optString);
  while (opt != -1) {
    switch (opt) {
        case 'k':
	  k = atoi(optarg);
	  break;
        case 's':
	  murmur3Seed = atoi(optarg);
	  break;
        case 'f':
	  fp = fopen(optarg, "r");
	  break;
    }
    opt = getopt(argc, argv, optString);
  }

  if (k < 2 || k > 15) {
    printf("Error: k must in the range 2-15. Use -k to set k.\n");
    exit(1);
  }
  if (fp == NULL) { 
    fp = stdin; 
  }

  double cardinalityEstimate = hyperLogLog(fp, k, murmur3Seed);

  printf("%.3lf\n", cardinalityEstimate);
  return 0;
}

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
    estimate = (-1.0 * max) * log(1.0 - estimate/j);
  }

  free(hash);
  return estimate;
}
