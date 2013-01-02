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
  fclose(fp);
  printf("%.3lf\n", cardinalityEstimate);
  return 0;
}
