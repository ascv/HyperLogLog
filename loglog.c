#include "loglog.h"

/*
 * To do: arguments: k, v, characters scanned, tests
 * rename leading zero count, add old hash functions back in
 */

int main(int argc, char ** argv) {

  FILE *fp;
  char buffer [100] = "";
  uint32_t k = 12;

  if ((fp = fopen(*++argv, "r")) == NULL) { 
    fp = stdin; 
  }

  u_int32_t size = 1 << k;
  u_int32_t M[size];

  uint32_t i;
  for (i = 0; i < size; i++) {
    M[i] = 0;
  }

  while ((fscanf(fp, "%100s", buffer)) == 1) {
    uint32_t hash = qhashmurmur3_32((void *) buffer, strlen(buffer));
    uint32_t index = hash >> (32 - k);
    uint32_t rank = lzc((hash << k) >> k) - k + 1;
    if (rank > M[index])
      M[index] = rank;
  }

  fclose(fp);

  double alpha_m = 0.0;
  switch (size) {
  case 16:
    alpha_m = 0.673;
    break;
  case 32:
    alpha_m = 0.697;
    break;
  case 64:
    alpha_m = 0.709;
    break;
  default:
    alpha_m = 0.7213/(1.0 + 1.079/(double) size);
    break;
  }
  
  double sum = 0.0;
  for (i = 0; i < size; i++) {
    sum = sum + 1.0/pow(2, (double) M[i]);
  }

  double max = pow(2, 32);
  double estimate = alpha_m * pow(sum, -1) * pow(size, 2);
  uint32_t * intEstimate = (uint32_t *) &estimate;

  if (estimate <= 2.5 * size) {
    double oneBits = (double) hamming_distance(*intEstimate);
    if (oneBits != 0.0) {
      estimate = size * log(size/oneBits);
    }
  }

  if (estimate <= (1.0/3.0) * max) {
    estimate = estimate;
  }
  else if (estimate > (1.0/3.0) * max) {
    estimate = (-1.0 * max) * log(1.0 - estimate/max);
  }

  printf("%lf\n", estimate);
  return 1;
}
