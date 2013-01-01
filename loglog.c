#include "loglog.h"
#include <unistd.h>

/*
-s seed, defaults to a random number otherwise
-k number of rank buckets
-c maximum number of characters to read, defaults to 100
 */

int main(int argc, char ** argv) {

  char *optString = "s:k:c:f:";
  //char *readBuffer = NULL;
  FILE * fp = NULL;
  uint32_t * hash = (uint32_t *) malloc(sizeof(uint32_t));
  //uint32_t * ranks = NULL;
  uint32_t index;
  uint32_t k = -1;
  uint32_t maxCharacters = 100;
  uint32_t murmur3Seed = 314;
  uint32_t opt;
  uint32_t rank;
  uint32_t size;

  opt = getopt(argc, argv, optString);
  while (opt != -1) {
    switch (opt) {
        case 'c':
	  maxCharacters = atoi(optarg);
	  break;
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
    opt = getopt( argc, argv, optString );
  }
  
  //printf("chars: %d seed: %d k: %d\n", maxCharacters, murmur3Seed, k);

  if (k == -1) {
    printf("Error: k not set. Use -k to set k.\n");
    exit(1);
  }
  if (k < 2 || k > 15) {
    printf("Error: k must in the range 2-15.\n");
    exit(1);
  }
  if (maxCharacters <= 0) {
    printf("Error: the maximum number of characters to read must greater than zero.\n");
    exit(1);
  }
  if (fp == NULL) { 
    fp = stdin; 
  }

  size = 1 << k;
  uint32_t M [size];
  char buffer [100] = "";
  //uint32_t * readBuffer = (uint32_t *) malloc(sizeof(uint32_t) * maxCharacters);

  uint32_t i;
  for (i = 0; i < size; i++) {
    M[i] = 0;
  }

  while ((fscanf(fp, "%100s", buffer)) == 1) {
    MurmurHash3_x86_32((void *) buffer, strlen(buffer), 42, (void *) hash);
    index = *hash >> (32 - k);
    rank = leadingZeroCount((*hash << k) >> k) - k + 1;
    if (rank > M[index])
      M[index] = rank;
  }

  free(hash);
  fclose(fp);

  double alphaM = 0.0;
  switch (size) {
  case 16:
    alphaM = 0.673;
    break;
  case 32:
    alphaM = 0.697;
    break;
  case 64:
    alphaM = 0.709;
    break;
  default:
    alphaM = 0.7213/(1.0 + 1.079/(double) size);
    break;
  }
  
  double sum = 0.0;
  for (i = 0; i < size; i++) {
    sum = sum + 1.0/pow(2, (double) M[i]);
  }

  double max = (double) 0x7FFFFFFF;
  double estimate = alphaM * pow(sum, -1) * pow(size, 2);
  uint32_t * intEstimate = (uint32_t *) &estimate;

  if (estimate <= 2.5 * size) {
    uint32_t zeros = 32 - ones(*intEstimate);
    if (zeros != 0) {
      estimate = size * log(size/zeros);
    }
  }

  if (estimate <= (1.0/3.0) * max) {
    estimate = estimate;
  }
  else if (estimate > (1.0/3.0) * max) {
    estimate = (-1.0 * max) * log(1.0 - estimate/max);
  }

  printf("%.3lf\n", estimate);
  return 0;
}
