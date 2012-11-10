#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

uint32_t ones(uint32_t x);
uint32_t lzc(uint32_t x);
uint32_t hamming_distance(uint32_t x);
uint32_t qhashmurmur3_32(const void *data, size_t nbytes);
uint32_t qhashfnv1_32(const void *data, size_t nbytes);
uint32_t jenkins_one_at_a_time_hash(const void * data, size_t len);


